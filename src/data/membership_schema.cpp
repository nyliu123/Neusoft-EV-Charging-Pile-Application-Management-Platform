#include "membership_schema.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>

namespace ev {
Result<int> migrateMembership(QSqlDatabase &db)
{
    if (!db.transaction()) return Result<int>::fail(ErrorCode::StorageError, db.lastError().text());
    QSqlQuery q(db);
    const QStringList ddl {
        "CREATE TABLE IF NOT EXISTS membership_plans (plan_id INTEGER PRIMARY KEY, level TEXT NOT NULL CHECK(level IN ('VIP','SVIP')), months INTEGER NOT NULL CHECK(months IN (1,3,12)), recurring INTEGER NOT NULL CHECK(recurring IN (0,1)), price_cent INTEGER NOT NULL CHECK(price_cent>0), discount_bps INTEGER NOT NULL CHECK(discount_bps BETWEEN 1 AND 9999), active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)), version INTEGER NOT NULL DEFAULT 1, UNIQUE(level,months,recurring))",
        "CREATE TABLE IF NOT EXISTS membership_operations (operation_id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES users(user_id), operation_key TEXT NOT NULL, fingerprint TEXT NOT NULL, plan_id INTEGER NOT NULL REFERENCES membership_plans(plan_id), price_cent INTEGER NOT NULL, created_at INTEGER NOT NULL, result_json TEXT NOT NULL, UNIQUE(user_id,operation_key))",
        "CREATE TABLE IF NOT EXISTS membership_entitlements (entitlement_id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES users(user_id), level TEXT NOT NULL CHECK(level IN ('VIP','SVIP')), starts_at INTEGER NOT NULL, expires_at INTEGER NOT NULL CHECK(expires_at>starts_at), discount_bps INTEGER NOT NULL CHECK(discount_bps BETWEEN 1 AND 9999), plan_id INTEGER NOT NULL REFERENCES membership_plans(plan_id), version INTEGER NOT NULL, operation_id INTEGER NOT NULL UNIQUE REFERENCES membership_operations(operation_id))",
        "CREATE INDEX IF NOT EXISTS idx_membership_user_time ON membership_entitlements(user_id,starts_at,expires_at)",
        "CREATE TABLE IF NOT EXISTS membership_events (event_id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES users(user_id), event_type TEXT NOT NULL, terms_json TEXT NOT NULL, created_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS membership_subscriptions (subscription_id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL UNIQUE REFERENCES users(user_id), plan_id INTEGER NOT NULL REFERENCES membership_plans(plan_id), version INTEGER NOT NULL, price_cent INTEGER NOT NULL, discount_bps INTEGER NOT NULL, next_due INTEGER NOT NULL, status TEXT NOT NULL, updated_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS knowledge_articles (article_id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, keywords TEXT NOT NULL, content TEXT NOT NULL, source TEXT NOT NULL, draft_version INTEGER NOT NULL DEFAULT 1, published_title TEXT NOT NULL DEFAULT '', published_keywords TEXT NOT NULL DEFAULT '', published_content TEXT NOT NULL DEFAULT '', published_source TEXT NOT NULL DEFAULT '', published_version INTEGER NOT NULL DEFAULT 0, active INTEGER NOT NULL DEFAULT 0, updated_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS feature_migrations (name TEXT PRIMARY KEY)"
    };
    auto fail = [&]() { QString error = q.lastError().text(); db.rollback(); return Result<int>::fail(ErrorCode::StorageError, error); };
    for (const auto &sql : ddl) if (!q.exec(sql)) return fail();
    QSet<QString> columns;
    if (!q.exec("PRAGMA table_info(orders)")) return fail();
    while (q.next()) columns.insert(q.value(1).toString());
    const QList<QPair<QString, QString>> added {
        {"membership_level", "TEXT NOT NULL DEFAULT 'NORMAL'"},
        {"discount_bps", "INTEGER NOT NULL DEFAULT 10000"},
        {"membership_version", "INTEGER NOT NULL DEFAULT 0"},
        {"gross_fee_cent", "INTEGER NOT NULL DEFAULT 0"}
    };
    for (const auto &column : added) {
        if (!columns.contains(column.first)
            && !q.exec("ALTER TABLE orders ADD COLUMN " + column.first + " " + column.second)) return fail();
    }
    if (!columns.contains("gross_fee_cent")
        && !q.exec("UPDATE orders SET gross_fee_cent=CAST(ROUND(total_fee*100) AS INTEGER)")) return fail();
    int id = 0;
    for (const QString &level : {QString("VIP"), QString("SVIP")}) {
        for (int recurring : {0, 1}) {
            for (int months : {1, 3, 12}) {
                ++id;
                // Explicitly documented demo prices; administrators can version them.
                const int monthly = level == "VIP" ? (recurring ? 1200 : 1500) : (recurring ? 2500 : 3000);
                q.prepare("INSERT OR IGNORE INTO membership_plans(plan_id,level,months,recurring,price_cent,discount_bps) VALUES(?,?,?,?,?,?)");
                q.addBindValue(id); q.addBindValue(level); q.addBindValue(months);
                q.addBindValue(recurring); q.addBindValue(monthly * months);
                q.addBindValue(level == "VIP" ? 9000 : 8000);
                if (!q.exec()) return fail();
            }
        }
    }
    if (!q.exec("SELECT 1 FROM feature_migrations WHERE name='membership-knowledge-v1'")) return fail();
    const bool seeded = q.next();
    q.finish();
    if (!seeded) {
        const QList<QStringList> articles {
            {"充电操作", "充电,预约,开始,结束,取消,故障", "登录后找桩并选择空闲设备，确认预约后开始模拟充电。结束充电时通过模拟钱包结算，余额不足时先充值再完成结算。已预约但未开始的订单可以取消。设备故障请联系管理员，不要自行拆卸。", "项目内置教学说明：充电流程"},
            {"会员权益与续费", "会员,VIP,SVIP,套餐,续费,到期,折扣,优惠", "VIP享充电优惠，SVIP优惠更大并可使用AI咨询。套餐分固定期限和连续订阅，各有1、3、12个月。连续订阅必须单独同意，取消续费不取消已付费权益。具体价格、折扣和上下架状态以会员中心当前套餐为准。未到期不支持跨等级购买。", "项目内置教学说明：会员规则"},
            {"费用与订单", "费用,计费,价格,订单,支付,余额,钱包,扣费,充值", "模拟电量由功率和充电时长计算。订单按预约时保存的单价和会员优惠结算，显示原价、优惠金额和应付金额，优惠不叠加。会员后来到期或后台调整折扣，不改变已有订单标准。项目不进行真实支付，也不连接真实充电设备。", "项目内置教学说明：计费规则"}
        };
        for (const auto &a : articles) {
            q.prepare("INSERT INTO knowledge_articles(title,keywords,content,source,published_title,published_keywords,published_content,published_source,published_version,active,updated_at) VALUES(?,?,?,?,?,?,?,?,1,1,strftime('%s','now'))");
            for (int repeat = 0; repeat < 2; ++repeat) for (const auto &v : a) q.addBindValue(v);
            if (!q.exec()) return fail();
        }
        if (!q.exec("INSERT INTO feature_migrations VALUES('membership-knowledge-v1')")) return fail();
    }
    if (!db.commit()) { db.rollback(); return Result<int>::fail(ErrorCode::StorageError, "membership migration commit failed"); }
    return Result<int>::ok(1);
}
}
