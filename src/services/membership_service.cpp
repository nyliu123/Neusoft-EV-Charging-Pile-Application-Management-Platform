#include "membership_service.h"
#include "sql_support.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace ev {
using namespace sqlsupport;
namespace {
void event(QSqlDatabase &db, qint64 user, const QString &type, const QJsonObject &terms, qint64 at) {
    query(db, "INSERT INTO membership_events(user_id,event_type,terms_json,created_at) VALUES(?,?,?,?)",
          {user,type,QString::fromUtf8(QJsonDocument(terms).toJson(QJsonDocument::Compact)),at});
}
JsonResult account(QSqlDatabase &db, qint64 user) {
    auto q = query(db, "SELECT status FROM users WHERE user_id=?", {user});
    if (!q.next()) return JsonResult::fail(ErrorCode::Unauthorized, QStringLiteral("请先登录"));
    if (q.value(0).toString() != "normal") return JsonResult::fail(ErrorCode::AccountFrozen, QStringLiteral("账号已冻结"));
    return JsonResult::ok({});
}
QJsonObject plan(QSqlDatabase &db, qint64 id) {
    auto q = query(db, "SELECT * FROM membership_plans WHERE plan_id=?", {id});
    return q.next() ? row(q) : QJsonObject{};
}
QJsonObject lastEntitlement(QSqlDatabase &db, qint64 user, qint64 at) {
    auto q = query(db, "SELECT * FROM membership_entitlements WHERE user_id=? AND expires_at>? ORDER BY expires_at DESC LIMIT 1", {user, at});
    return q.next() ? row(q) : QJsonObject{};
}
// Called only inside an IMMEDIATE transaction. One ledger entry, one debit, one entitlement.
JsonResult grant(QSqlDatabase &db, qint64 user, const QJsonObject &p, qint64 start,
                 qint64 end, const QString &key, const QString &fingerprint, qint64 at) {
    const qint64 cost = p.value("price_cent").toInteger();
    auto debit = query(db, "UPDATE users SET balance=(CAST(ROUND(balance*100) AS INTEGER)-?)/100.0 WHERE user_id=? AND status='normal' AND CAST(ROUND(balance*100) AS INTEGER)>=?", {cost,user,cost});
    if (debit.numRowsAffected() != 1) return JsonResult::fail(ErrorCode::InsufficientBalance, QStringLiteral("模拟钱包余额不足，请先充值；本次未扣费"));
    QJsonObject result{{"level",p.value("level")},{"starts_at",start},{"expires_at",end},{"paid_cent",cost}};
    auto balance = query(db, "SELECT CAST(ROUND(balance*100) AS INTEGER) FROM users WHERE user_id=?", {user});
    balance.next(); result.insert("balance_cent", balance.value(0).toLongLong());
    auto op = query(db, "INSERT INTO membership_operations(user_id,operation_key,fingerprint,plan_id,price_cent,created_at,result_json) VALUES(?,?,?,?,?,?,?)",
        {user,key,fingerprint,p.value("plan_id").toInteger(),cost,at,QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))});
    query(db, "INSERT INTO membership_entitlements(user_id,level,starts_at,expires_at,discount_bps,plan_id,version,operation_id) VALUES(?,?,?,?,?,?,?,?)",
        {user,p.value("level").toString(),start,end,p.value("discount_bps").toInt(),p.value("plan_id").toInteger(),p.value("version").toInt(),op.lastInsertId()});
    return JsonResult::ok(result);
}
}

qint64 MembershipService::addMonths(qint64 from, int months) {
    // Use a fixed China civil timezone, independent of server locale/DST.
    const auto date = QDateTime::fromSecsSinceEpoch(from, Qt::OffsetFromUTC, 8 * 3600).addMonths(months);
    return date.isValid() ? date.toSecsSinceEpoch() : 0;
}

MembershipService::JsonResult MembershipService::plans(QSqlDatabase &db, bool admin) const {
    return guard(db, false, [&] {
        auto q = query(db, "SELECT * FROM membership_plans" + QString(admin ? "" : " WHERE active=1") + " ORDER BY level DESC,recurring,months");
        QJsonArray plans; while (q.next()) plans.append(row(q));
        return JsonResult::ok({{"plans",plans},{"demo_only",true}});
    });
}

MembershipService::JsonResult MembershipService::snapshot(QSqlDatabase &db, qint64 user, qint64 at) const {
    return guard(db, false, [&] {
        auto q = query(db, "SELECT level,discount_bps,version,starts_at,expires_at FROM membership_entitlements WHERE user_id=? AND starts_at<=? AND expires_at>? ORDER BY starts_at DESC LIMIT 1", {user,at,at});
        QJsonObject result = q.next() ? row(q) : QJsonObject{{"level","NORMAL"},{"discount_bps",10000},{"version",0},{"starts_at",0},{"expires_at",0}};
        result.insert("valid", result.value("level").toString() != "NORMAL");
        return JsonResult::ok(result);
    });
}

MembershipService::JsonResult MembershipService::status(QSqlDatabase &db, qint64 user, qint64 at) const {
    return guard(db, false, [&] {
        const auto auth = account(db,user); if (!auth.success) return auth;
        auto result = snapshot(db,user,at); if (!result.success) return result;
        const auto last = lastEntitlement(db,user,at);
        result.data.insert("paid_until",last.value("expires_at"));
        auto sub = query(db, "SELECT subscription_id,plan_id,version,price_cent,discount_bps,next_due,status FROM membership_subscriptions WHERE user_id=?", {user});
        result.data.insert("renewal", sub.next() ? row(sub) : QJsonObject{});
        auto history = query(db, "SELECT operation_id,plan_id,price_cent,created_at,result_json FROM membership_operations WHERE user_id=? ORDER BY operation_id DESC LIMIT 30", {user});
        QJsonArray records;
        while (history.next()) { auto record = row(history); record.remove("result_json"); records.append(record); }
        result.data.insert("history", records);
        return result;
    });
}

MembershipService::JsonResult MembershipService::purchase(QSqlDatabase &db, qint64 user, const QJsonObject &params, qint64 at) const {
    const QString key = params.value("operation_key").toString();
    if (key.startsWith("renewal_") || !integer(params,"plan_id",1,12) || !integer(params,"version",1,1000000000)
        || !QRegularExpression("^[A-Za-z0-9_-]{8,80}$").match(key).hasMatch())
        return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("购买参数或操作编号无效"));
    const QJsonObject terms{{"plan_id",params.value("plan_id")},{"version",params.value("version")},{"renewal_consent",params.value("renewal_consent").toBool()}};
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(terms).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    return guard(db, true, [&] {
        auto auth = account(db,user); if (!auth.success) return auth;
        auto prior = query(db, "SELECT fingerprint,result_json FROM membership_operations WHERE user_id=? AND operation_key=?", {user,key});
        if (prior.next()) {
            if (prior.value(0).toString() != fingerprint) return JsonResult::fail(ErrorCode::RequestConflict, QStringLiteral("同一操作编号不能用于不同购买"));
            auto replay = QJsonDocument::fromJson(prior.value(1).toByteArray()).object(); replay.insert("replayed",true);
            // The purchase outcome is immutable, but the wallet may have changed since it.
            auto balance = query(db,"SELECT CAST(ROUND(balance*100) AS INTEGER) FROM users WHERE user_id=?",{user});
            balance.next(); replay.insert("balance_cent",balance.value(0).toLongLong());
            return JsonResult::ok(replay);
        }
        const auto p = plan(db,params.value("plan_id").toInteger());
        if (p.isEmpty() || p.value("active").toInt() != 1) return JsonResult::fail(ErrorCode::StateConflict, QStringLiteral("套餐已下架，请刷新"));
        if (p.value("version") != params.value("version")) return JsonResult::fail(ErrorCode::StateConflict, QStringLiteral("套餐价格或权益已更新，请重新确认"));
        const bool recurring = p.value("recurring").toInt() == 1;
        if (recurring && !terms.value("renewal_consent").toBool()) return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("连续套餐需要单独同意续费价格和周期"));
        const auto last = lastEntitlement(db,user,at);
        if (!last.isEmpty() && last.value("level") != p.value("level")) return JsonResult::fail(ErrorCode::StateConflict, QStringLiteral("当前会员尚未到期，暂不支持跨等级购买"));
        const qint64 start = last.isEmpty() ? at : last.value("expires_at").toInteger();
        const qint64 end = addMonths(start,p.value("months").toInt());
        if (end <= start || end > at + 20LL*366*86400) return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("会员期限超出支持范围"));
        auto result = grant(db,user,p,start,end,key,fingerprint,at);
        if (!result.success) return result;
        auto consent = p;
        consent.insert("renewal_consent",recurring);
        consent.insert("operation_key",key);
        event(db,user,recurring?"purchase_and_authorize":"purchase_fixed",consent,at);
        if (recurring) {
            query(db, "INSERT INTO membership_subscriptions(user_id,plan_id,version,price_cent,discount_bps,next_due,status,updated_at) VALUES(?,?,?,?,?,?,'active',?) ON CONFLICT(user_id) DO UPDATE SET plan_id=excluded.plan_id,version=excluded.version,price_cent=excluded.price_cent,discount_bps=excluded.discount_bps,next_due=excluded.next_due,status='active',updated_at=excluded.updated_at",
                {user,p.value("plan_id").toInteger(),p.value("version").toInt(),p.value("price_cent").toInteger(),p.value("discount_bps").toInt(),end,at});
        } else {
            query(db, "UPDATE membership_subscriptions SET next_due=?,updated_at=?,status=CASE WHEN ? THEN status ELSE 'cancelled' END WHERE user_id=?", {end,at,!last.isEmpty(),user});
        }
        return result;
    });
}

MembershipService::JsonResult MembershipService::setRenewal(QSqlDatabase &db, qint64 user, const QJsonObject &params, qint64 at) const {
    if (!params.value("enabled").isBool()) return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("必须明确选择续费开关"));
    return guard(db, true, [&] {
        auto auth = account(db,user); if (!auth.success) return auth;
        if (!params.value("enabled").toBool()) {
            query(db, "UPDATE membership_subscriptions SET status='cancelled',updated_at=? WHERE user_id=?",{at,user});
            event(db,user,"cancel_renewal",{{"enabled",false}},at);
            return JsonResult::ok({{"cancelled",true}});
        }
        if (!integer(params,"plan_id",1,12) || !integer(params,"version",1,1000000000) || !params.value("renewal_consent").toBool())
            return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("请确认新的套餐价格和续费周期"));
        const auto p = plan(db,params.value("plan_id").toInteger());
        const auto last = lastEntitlement(db,user,at);
        if (last.isEmpty() || p.value("level") != last.value("level") || p.value("recurring").toInt()!=1 || p.value("active").toInt()!=1 || p.value("version")!=params.value("version"))
            return JsonResult::fail(ErrorCode::StateConflict, QStringLiteral("请刷新套餐；会员已到期时需重新购买，未到期仅可授权同等级连续套餐"));
        query(db, "INSERT INTO membership_subscriptions(user_id,plan_id,version,price_cent,discount_bps,next_due,status,updated_at) VALUES(?,?,?,?,?,?,'active',?) ON CONFLICT(user_id) DO UPDATE SET plan_id=excluded.plan_id,version=excluded.version,price_cent=excluded.price_cent,discount_bps=excluded.discount_bps,next_due=excluded.next_due,status='active',updated_at=excluded.updated_at",
            {user,p.value("plan_id").toInteger(),p.value("version").toInt(),p.value("price_cent").toInteger(),p.value("discount_bps").toInt(),last.value("expires_at").toInteger(),at});
        auto consent=p; consent.insert("renewal_consent",true);
        event(db,user,"authorize_renewal",consent,at);
        return JsonResult::ok({{"enabled",true},{"next_due",last.value("expires_at")}});
    });
}

MembershipService::JsonResult MembershipService::updatePlan(QSqlDatabase &db, const QJsonObject &params) const {
    if (!integer(params,"plan_id",1,12) || !integer(params,"version",1,1000000000) || !integer(params,"price_cent",1,100000000) || !integer(params,"discount_bps",1,9999) || !params.value("active").isBool())
        return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("价格必须大于0，折扣须在0到10折之间，请刷新后重试"));
    return guard(db, true, [&] {
        const auto p = plan(db,params.value("plan_id").toInteger());
        if (p.isEmpty() || p.value("version")!=params.value("version")) return JsonResult::fail(ErrorCode::StateConflict, QStringLiteral("套餐已被其他管理员修改，请刷新"));
        const int bps = params.value("discount_bps").toInt();
        auto other = query(db, p.value("level").toString()=="VIP" ? "SELECT MAX(discount_bps) FROM membership_plans WHERE level='SVIP'" : "SELECT MIN(discount_bps) FROM membership_plans WHERE level='VIP'");
        other.next();
        if ((p.value("level").toString()=="VIP" && bps<=other.value(0).toInt()) || (p.value("level").toString()=="SVIP" && bps>=other.value(0).toInt()))
            return JsonResult::fail(ErrorCode::InvalidInput, QStringLiteral("每种SVIP套餐的优惠都必须大于VIP（应付折扣更低）"));
        const bool changed = p.value("price_cent")!=params.value("price_cent") || p.value("discount_bps")!=params.value("discount_bps") || (p.value("active").toInt()==1)!=params.value("active").toBool();
        if (!changed) return JsonResult::ok(p);
        query(db, "UPDATE membership_plans SET price_cent=?,discount_bps=?,active=?,version=version+1 WHERE plan_id=?",
            {params.value("price_cent").toInteger(),bps,params.value("active").toBool(),params.value("plan_id").toInteger()});
        query(db, "UPDATE membership_subscriptions SET status='needs_confirmation',updated_at=? WHERE plan_id=? AND status='active'", {now(),params.value("plan_id").toInteger()});
        return JsonResult::ok(plan(db,params.value("plan_id").toInteger()));
    });
}

MembershipService::JsonResult MembershipService::processDue(QSqlDatabase &db, qint64 at) const {
    return guard(db, true, [&] {
        auto q = query(db, "SELECT * FROM membership_subscriptions WHERE status='active' AND next_due<=? ORDER BY next_due LIMIT 100", {at});
        QList<QJsonObject> due; while(q.next()) due.append(row(q)); q.finish();
        int paid = 0;
        for (const auto &s : due) {
            const qint64 user = s.value("user_id").toInteger();
            const auto p = plan(db,s.value("plan_id").toInteger());
            const qint64 start = s.value("next_due").toInteger(), end = addMonths(start,p.value("months").toInt());
            QString stopped;
            if (!account(db,user).success) stopped = "account_unavailable";
            else if (p.value("active").toInt()!=1 || p.value("version")!=s.value("version")) stopped = "needs_confirmation";
            else if (end<=at) stopped = "missed_cycle"; // Never debit an entirely elapsed period after downtime.
            if (!stopped.isEmpty()) {
                event(db,user,"renewal_stopped",{{"reason",stopped},{"next_due",start},{"plan_id",s.value("plan_id")}},at);
                query(db,"UPDATE membership_subscriptions SET status=?,updated_at=? WHERE subscription_id=?",{stopped,at,s.value("subscription_id").toInteger()});
                continue;
            }
            const QString key = QString("renewal_%1_%2").arg(s.value("subscription_id").toInteger()).arg(start);
            auto exists = query(db,"SELECT 1 FROM membership_operations WHERE user_id=? AND operation_key=?",{user,key});
            if (exists.next()) { query(db,"UPDATE membership_subscriptions SET next_due=?,updated_at=? WHERE subscription_id=?",{end,at,s.value("subscription_id").toInteger()}); continue; }
            const auto result = grant(db,user,p,start,end,key,key,at);
            if (!result.success) {
                if (result.code!=ErrorCode::InsufficientBalance) return result;
                query(db,"UPDATE membership_subscriptions SET status='insufficient_balance',updated_at=? WHERE subscription_id=?",{at,s.value("subscription_id").toInteger()});
                event(db,user,"renewal_stopped",{{"reason","insufficient_balance"},{"next_due",start},{"plan_id",s.value("plan_id")}},at);
                continue;
            }
            query(db,"UPDATE membership_subscriptions SET next_due=?,updated_at=? WHERE subscription_id=?",{end,at,s.value("subscription_id").toInteger()});
            event(db,user,"renewal_paid",{{"operation_key",key},{"terms",p},{"starts_at",start},{"expires_at",end}},at);
            ++paid;
        }
        return JsonResult::ok({{"renewed",paid}});
    });
}
}
