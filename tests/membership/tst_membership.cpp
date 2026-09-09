#include "data/database_manager.h"
#include "services/membership_service.h"
#include "services/knowledge_service.h"
#include "services/charge_service.h"
#include "services/order_service.h"
#include "services/fee_calculator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class MembershipTests final : public QObject {
    Q_OBJECT
    QSqlDatabase db;
    std::unique_ptr<QTemporaryDir> directory;
    ev::MembershipService service;
    ev::KnowledgeService knowledge;
    // Fixed test clock; production uses the actual server clock.
    const qint64 at = QDateTime(QDate(2026,1,31),QTime(12,0),Qt::OffsetFromUTC,28800).toSecsSinceEpoch();
    qint64 scalar(const QString &sql) {
        QSqlQuery q(db);
        if (!q.exec(sql) || !q.next()) { QTest::qFail(qPrintable(q.lastError().text()),__FILE__,__LINE__); return -1; }
        return q.value(0).toLongLong();
    }
    void exec(const QString &sql) {
        QSqlQuery q(db);
        QVERIFY2(q.exec(sql),qPrintable(q.lastError().text()));
    }
    QJsonObject params(int plan=1,const QString &key="purchase-test-001",int version=1,bool consent=false) {
        return {{"plan_id",plan},{"version",version},{"operation_key",key},{"renewal_consent",consent}};
    }
    QJsonObject renewal() { return service.status(db,1,at).data.value("renewal").toObject(); }
    qint64 balance() { return scalar("SELECT ROUND(balance*100) FROM users WHERE user_id=1"); }
    void changePlan(int id,int price,int bps,int version=1,bool active=true) {
        const auto r=service.updatePlan(db,{{"plan_id",id},{"version",version},{"price_cent",price},{"discount_bps",bps},{"active",active}});
        QVERIFY2(r.success,qPrintable(r.message));
    }
private slots:
    void init() {
        directory=std::make_unique<QTemporaryDir>();
        QVERIFY(directory->isValid());
        ev::DatabaseManager manager(directory->filePath("membership.sqlite3"));
        const auto opened=manager.openForCurrentThread();
        QVERIFY2(opened.success,qPrintable(opened.message)); db=opened.data;
        const auto migrated=manager.migrate(db); QVERIFY2(migrated.success,qPrintable(migrated.message));
        exec("UPDATE users SET balance=10000,status='normal' WHERE user_id=1");
    }
    void cleanup() {
        const auto name=db.connectionName(); db.close(); db=QSqlDatabase();
        QSqlDatabase::removeDatabase(name); directory.reset();
    }
    void migrationIsAdditiveAndRepeatable() {
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_plans"),12);
        QCOMPARE(scalar("SELECT COUNT(*) FROM (SELECT DISTINCT level,months,recurring FROM membership_plans)"),12);
        QCOMPARE(scalar("SELECT COUNT(*) FROM orders WHERE membership_level<>'NORMAL' OR discount_bps<>10000 OR gross_fee_cent<>ROUND(total_fee*100)"),0);
        const auto old=scalar("SELECT COUNT(*) FROM orders");
        changePlan(1,1700,8900);
        exec("UPDATE knowledge_articles SET active=0 WHERE article_id=1");
        ev::DatabaseManager manager(db.databaseName());
        QVERIFY(manager.migrate(db).success);
        QCOMPARE(scalar("SELECT COUNT(*) FROM orders"),old);
        QCOMPARE(scalar("SELECT price_cent FROM membership_plans WHERE plan_id=1"),1700);
        QCOMPARE(scalar("SELECT active FROM knowledge_articles WHERE article_id=1"),0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM knowledge_articles"),3);
    }
    void naturalMonthsClamp_data() {
        QTest::addColumn<QString>("from"); QTest::addColumn<int>("months"); QTest::addColumn<QString>("until");
        QTest::newRow("month-end") << "2026-01-31" << 1 << "2026-02-28";
        QTest::newRow("leap-February") << "2024-01-31" << 1 << "2024-02-29";
        QTest::newRow("quarter") << "2026-01-31" << 3 << "2026-04-30";
        QTest::newRow("leap-year-to-normal") << "2024-02-29" << 12 << "2025-02-28";
    }
    void naturalMonthsClamp() {
        QFETCH(QString,from);QFETCH(int,months);QFETCH(QString,until);
        const auto start=QDateTime(QDate::fromString(from,Qt::ISODate),QTime(13,25,30),Qt::OffsetFromUTC,28800);
        const auto end=QDateTime::fromSecsSinceEpoch(service.addMonths(start.toSecsSinceEpoch(),months),Qt::OffsetFromUTC,28800);
        QCOMPARE(end.date(),QDate::fromString(until,Qt::ISODate)); QCOMPARE(end.time(),start.time());
    }
    void fixedPurchaseIsAtomicIdempotentAndPrivate() {
        const auto p=params(); const auto r=service.purchase(db,1,p,at);
        QVERIFY2(r.success,qPrintable(r.message)); QCOMPARE(balance(),998500);
        QCOMPARE(r.data.value("expires_at").toInteger(),service.addMonths(at,1));
        QCOMPARE(service.snapshot(db,1,at).data.value("level").toString(),QString("VIP"));
        QCOMPARE(service.snapshot(db,2,at).data.value("level").toString(),QString("NORMAL"));
        QVERIFY(renewal().isEmpty());
        changePlan(1,1700,8900,1,false);
        const auto again=service.purchase(db,1,p,at+1);
        QVERIFY(again.success);QVERIFY(again.data.value("replayed").toBool()); QCOMPARE(balance(),998500);
        QCOMPARE(again.data.value("expires_at"),r.data.value("expires_at"));
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_operations"),1);
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_events"),1);
        exec("UPDATE users SET balance=9000 WHERE user_id=1");
        const auto afterOtherSpending=service.purchase(db,1,p,at+2);
        QVERIFY(afterOtherSpending.success);
        QCOMPARE(afterOtherSpending.data.value("balance_cent").toInteger(),900000);
        QCOMPARE(balance(),900000); // Replay never restores or displays an obsolete wallet balance.
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_operations"),1);
        auto conflict=p;conflict.insert("plan_id",2);
        QCOMPARE(service.purchase(db,1,conflict,at).code,ev::ErrorCode::RequestConflict);
        auto reserved=params(1,"renewal_1_1234");
        QCOMPARE(service.purchase(db,1,reserved,at).code,ev::ErrorCode::InvalidInput);
    }
    void failedGrantRollsBackDebitAndLedger() {
        exec("CREATE TRIGGER fail_membership BEFORE INSERT ON membership_entitlements BEGIN SELECT RAISE(ABORT,'injected failure'); END");
        const auto result=service.purchase(db,1,params(),at);
        QCOMPARE(result.code,ev::ErrorCode::StorageError);QCOMPARE(balance(),1000000);
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_operations"),0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_events"),0);
        exec("DROP TRIGGER fail_membership");QVERIFY(service.purchase(db,1,params(),at).success);
    }
    void invalidOfflineStaleAndInsufficientNeverDebit() {
        auto p=params();p.insert("plan_id",1.5);QVERIFY(!service.purchase(db,1,p,at).success);
        p=params();p.insert("version",2);QCOMPARE(service.purchase(db,1,p,at).code,ev::ErrorCode::StateConflict);
        changePlan(1,1500,9000,1,false);QVERIFY(!service.purchase(db,1,params(),at).success);
        QCOMPARE(balance(),1000000);
        exec("UPDATE users SET balance=1 WHERE user_id=1");
        QCOMPARE(service.purchase(db,1,params(2),at).code,ev::ErrorCode::InsufficientBalance);QCOMPARE(balance(),100);
        exec("UPDATE users SET status='frozen' WHERE user_id=1");
        QCOMPARE(service.purchase(db,1,params(2),at).code,ev::ErrorCode::AccountFrozen);
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_operations"),0);
    }
    void renewalExtendsAndPreservesOldBenefits() {
        auto first=service.purchase(db,1,params(),at);QVERIFY(first.success);
        auto end=first.data.value("expires_at").toInteger();
        changePlan(1,1600,8800);
        auto second=service.purchase(db,1,params(1,"purchase-test-002",2),at+10);QVERIFY(second.success);
        QCOMPARE(second.data.value("starts_at").toInteger(),end);
        QCOMPARE(service.snapshot(db,1,end-1).data.value("discount_bps").toInt(),9000);
        QCOMPARE(service.snapshot(db,1,end).data.value("discount_bps").toInt(),8800);
        auto paidUntil=second.data.value("expires_at").toInteger();
        QCOMPARE(service.status(db,1,at).data.value("paid_until").toInteger(),paidUntil);
        QCOMPARE(service.snapshot(db,1,paidUntil).data.value("level").toString(),QString("NORMAL"));
        QCOMPARE(service.purchase(db,1,params(7,"purchase-test-003"),at+20).code,ev::ErrorCode::StateConflict);
        QVERIFY(service.purchase(db,1,params(7,"purchase-test-003"),paidUntil).success);
    }
    void recurringRequiresSeparateConsentAndAuditsIt() {
        auto p=params(4);QCOMPARE(service.purchase(db,1,p,at).code,ev::ErrorCode::InvalidInput);
        QCOMPARE(balance(),1000000);p.insert("renewal_consent",true);
        QVERIFY(service.purchase(db,1,p,at).success);QCOMPARE(balance(),998800);
        QCOMPARE(renewal().value("status").toString(),QString("active"));
        QSqlQuery q(db);QVERIFY(q.exec("SELECT terms_json FROM membership_events"));QVERIFY(q.next());
        const auto consent=QJsonDocument::fromJson(q.value(0).toByteArray()).object();
        QVERIFY(consent.value("renewal_consent").toBool());QCOMPARE(consent.value("price_cent").toInt(),1200);
        QCOMPARE(consent.value("months").toInt(),1);QCOMPARE(consent.value("version").toInt(),1);
    }
    void dueCycleOnlyDebitsOnceAndCancelKeepsPaidRights() {
        auto r=service.purchase(db,1,params(4,"purchase-test-001",1,true),at);QVERIFY(r.success);
        const auto due=r.data.value("expires_at").toInteger();
        QCOMPARE(service.processDue(db,due-1).data.value("renewed").toInt(),0);
        QCOMPARE(service.processDue(db,due).data.value("renewed").toInt(),1);QCOMPARE(balance(),997600);
        QCOMPARE(service.processDue(db,due+5).data.value("renewed").toInt(),0);QCOMPARE(balance(),997600);
        QVERIFY(service.setRenewal(db,1,{{"enabled",false}},due+6).success);
        const auto next=service.addMonths(due,1);
        QCOMPARE(service.snapshot(db,1,next-1).data.value("level").toString(),QString("VIP"));
        QCOMPARE(service.processDue(db,next).data.value("renewed").toInt(),0);QCOMPARE(balance(),997600);
        QCOMPARE(scalar("SELECT COUNT(*) FROM membership_operations"),2);
    }
    void cancelBeforeDueAndFixedPurchaseDoesNotEnableRenewal() {
        auto r=service.purchase(db,1,params(4,"purchase-test-001",1,true),at);QVERIFY(r.success);
        QVERIFY(service.setRenewal(db,1,{{"enabled",false}},at+1).success);
        QVERIFY(service.purchase(db,1,params(1,"purchase-test-002"),at+2).success);
        QCOMPARE(renewal().value("status").toString(),QString("cancelled"));
        QCOMPARE(service.processDue(db,service.addMonths(r.data.value("expires_at").toInteger(),1)).data.value("renewed").toInt(),0);
        QCOMPARE(balance(),997300);
    }
    void priceChangeRequiresNewConsent() {
        auto r=service.purchase(db,1,params(4,"purchase-test-001",1,true),at);QVERIFY(r.success);
        changePlan(4,1300,8900);
        QCOMPARE(renewal().value("status").toString(),QString("needs_confirmation"));
        const auto due=r.data.value("expires_at").toInteger();
        QCOMPARE(service.processDue(db,due).data.value("renewed").toInt(),0);QCOMPARE(balance(),998800);
        QJsonObject consent{{"enabled",true},{"plan_id",4},{"version",1},{"renewal_consent",true}};
        QVERIFY(!service.setRenewal(db,1,consent,at+10).success);
        consent.insert("version",2);QVERIFY(service.setRenewal(db,1,consent,at+10).success);
        QCOMPARE(service.processDue(db,due).data.value("renewed").toInt(),1);QCOMPARE(balance(),997500);
        QCOMPARE(service.snapshot(db,1,due-1).data.value("discount_bps").toInt(),9000);
        QCOMPARE(service.snapshot(db,1,due).data.value("discount_bps").toInt(),8900);
    }
    void insufficientRenewalExpiresAndDoesNotKeepRetrying() {
        auto r=service.purchase(db,1,params(4,"purchase-test-001",1,true),at);QVERIFY(r.success);
        const auto due=r.data.value("expires_at").toInteger();exec("UPDATE users SET balance=1 WHERE user_id=1");
        QVERIFY(service.processDue(db,due).success);QCOMPARE(balance(),100);
        QCOMPARE(renewal().value("status").toString(),QString("insufficient_balance"));
        QVERIFY(!service.snapshot(db,1,due).data.value("valid").toBool());
        exec("UPDATE users SET balance=10000 WHERE user_id=1");
        QCOMPARE(service.processDue(db,due+60).data.value("renewed").toInt(),0);QCOMPARE(balance(),1000000);
    }
    void restartDoesNotBillWhollyElapsedCycles() {
        auto r=service.purchase(db,1,params(4,"purchase-test-001",1,true),at);QVERIFY(r.success);
        const auto due=r.data.value("expires_at").toInteger();
        QCOMPARE(service.processDue(db,service.addMonths(due,1)).data.value("renewed").toInt(),0);
        QCOMPARE(renewal().value("status").toString(),QString("missed_cycle"));QCOMPARE(balance(),998800);
    }
    void adminValidationAndSnapshotVersioning() {
        QJsonObject p{{"plan_id",1},{"version",1},{"price_cent",0},{"discount_bps",9000},{"active",true}};
        QVERIFY(!service.updatePlan(db,p).success);p.insert("price_cent",1600);p.insert("discount_bps",8000);
        QVERIFY(!service.updatePlan(db,p).success);p.insert("discount_bps",9000);QVERIFY(service.updatePlan(db,p).success);
        QCOMPARE(service.updatePlan(db,p).code,ev::ErrorCode::StateConflict);
        p.insert("version",2);p.insert("active",false);QVERIFY(service.updatePlan(db,p).success);
        QCOMPARE(service.plans(db).data.value("plans").toArray().size(),11);
        QCOMPARE(service.plans(db,true).data.value("plans").toArray().size(),12);
    }
    void reservationDiscountSurvivesExpiryConfigAndPendingSettlement() {
        const auto now=service.now();QVERIFY(service.purchase(db,1,params(7),now-10).success);
        // Existing teaching orders belong to this fixture only; finish their state to reserve a fresh pile.
        exec("UPDATE orders SET status='settled' WHERE user_id=1");
        auto pile=scalar("SELECT pile_id FROM charging_piles WHERE status='idle' LIMIT 1");
        ev::ChargeService charging;
        const auto reserved=charging.reserve(db,1,pile);QVERIFY2(reserved.success,qPrintable(reserved.message));
        QCOMPARE(reserved.data.discountBps,8000);QCOMPARE(reserved.data.membershipLevel,QString("SVIP"));
        const auto id=reserved.data.orderId;
        exec(QString("UPDATE membership_entitlements SET starts_at=%1,expires_at=%2 WHERE user_id=1").arg(now-20).arg(now-1));
        changePlan(7,3200,7500);
        QVERIFY(charging.startCharge(db,1,id).success);
        exec(QString("UPDATE orders SET start_time=datetime('now','localtime','-10 minutes') WHERE order_id=%1").arg(id));
        exec("UPDATE users SET balance=0 WHERE user_id=1");
        auto pending=charging.endCharge(db,1,id);QVERIFY2(pending.success,qPrintable(pending.message));
        QVERIFY(!pending.data.settled);QVERIFY(pending.data.totalFeeCent>0);
        QCOMPARE(pending.data.discountBps,8000);QVERIFY(pending.data.grossFeeCent>pending.data.totalFeeCent);
        exec("UPDATE users SET balance=10000 WHERE user_id=1");
        auto settled=charging.endCharge(db,1,id);QVERIFY(settled.success);QVERIFY(settled.data.settled);
        QCOMPARE(settled.data.totalFeeCent,pending.data.totalFeeCent);QCOMPARE(settled.data.grossFeeCent,pending.data.grossFeeCent);
        QCOMPARE(balance(),1000000-settled.data.totalFeeCent);
        QCOMPARE(scalar(QString("SELECT discount_bps FROM orders WHERE order_id=%1").arg(id)),8000);
        auto normal=charging.reserve(db,1,pile);QVERIFY(normal.success);QCOMPARE(normal.data.discountBps,10000);
    }
    void knowledgePublicationKeepsOldVersionUntilSuccessfulPublish() {
        QJsonObject p{{"article_id",0},{"draft_version",0},{"title","test title"},{"keywords","uniquetopic"},{"content","first published content"},{"source","test handbook section 1"}};
        const auto saved=knowledge.save(db,p);QVERIFY(saved.success);p.insert("article_id",saved.data.value("article_id"));p.insert("draft_version",1);
        QVERIFY(knowledge.retrieve(db,"uniquetopic").data.value("sources").toArray().isEmpty());
        QVERIFY(knowledge.publish(db,p,true).success);
        QCOMPARE(knowledge.retrieve(db,"uniquetopic").data.value("sources").toArray().first().toObject().value("content").toString(),QString("first published content"));
        p.insert("content","unpublished revision");QVERIFY(knowledge.save(db,p).success);
        QVERIFY(!knowledge.publish(db,p,true).success);
        QCOMPARE(knowledge.retrieve(db,"uniquetopic").data.value("sources").toArray().first().toObject().value("version").toInt(),1);
        p.insert("draft_version",2);QVERIFY(knowledge.publish(db,p,true).success);
        QCOMPARE(knowledge.retrieve(db,"uniquetopic").data.value("sources").toArray().first().toObject().value("content").toString(),QString("unpublished revision"));
        QVERIFY(knowledge.publish(db,p,false).success);QVERIFY(knowledge.retrieve(db,"uniquetopic").data.value("sources").toArray().isEmpty());
        p.insert("content","");QVERIFY(!knowledge.save(db,p).success);
    }
};
QTEST_GUILESS_MAIN(MembershipTests)
#include "tst_membership.moc"
