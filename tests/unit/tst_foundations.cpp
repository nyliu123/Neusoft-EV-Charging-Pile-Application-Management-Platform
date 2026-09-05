#include "common/phone_validator.h"
#include "common/protocol.h"
#include "data/database_manager.h"
#include "network/frame_codec.h"
#include "services/fee_calculator.h"
#include "services/session_manager.h"
#include "services/user_service.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

class FoundationTests final : public QObject {
    Q_OBJECT

private slots:
    void frameRoundTrip();
    void frameWaitsForCompletePayload();
    void invalidJsonConsumesOnlyItsFrame();
    void feeUsesOrderSnapshotAndHalfUpRounding();
    void feeRejectsInvalidDiscount();
    void databaseCreatesCoreSchema();
    void phoneValidation_data();
    void phoneValidation();
    void phoneValidationMessages();
    void existingUserLoginPaths();
    void automaticRegistrationCreatesDefaultsAndHandlesConflict();
    void sessionLifecycle();
};

void FoundationTests::frameRoundTrip()
{
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ev::ProtocolVersion)},
        {QStringLiteral("request_id"), QStringLiteral("req-001")},
        {QStringLiteral("data"), QJsonObject {{QStringLiteral("昵称"), QStringLiteral("用户0001")}}}
    };
    QByteArray buffer = ev::FrameCodec::encode(
        static_cast<quint32>(ev::MessageType::LoginRequest), payload);

    const ev::DecodeResult result = ev::FrameCodec::decodeOne(buffer);
    QCOMPARE(result.status, ev::DecodeStatus::Complete);
    QCOMPARE(result.frame.messageType,
             static_cast<quint32>(ev::MessageType::LoginRequest));
    QCOMPARE(result.frame.payload, payload);
    QVERIFY(buffer.isEmpty());
}

void FoundationTests::frameWaitsForCompletePayload()
{
    const QByteArray frame = ev::FrameCodec::encode(0x20, {{QStringLiteral("a"), 1}});
    QByteArray buffer = frame.left(frame.size() - 1);
    QCOMPARE(ev::FrameCodec::decodeOne(buffer).status, ev::DecodeStatus::NeedMore);
    QCOMPARE(buffer.size(), frame.size() - 1);
}

void FoundationTests::invalidJsonConsumesOnlyItsFrame()
{
    QByteArray invalid(8, '\0');
    qToBigEndian<quint32>(0x10, invalid.data());
    qToBigEndian<quint32>(1, invalid.data() + 4);
    invalid.append('{');
    QByteArray buffer = invalid + ev::FrameCodec::encode(0x20, {{QStringLiteral("ok"), true}});

    QCOMPARE(ev::FrameCodec::decodeOne(buffer).status, ev::DecodeStatus::Invalid);
    QCOMPARE(ev::FrameCodec::decodeOne(buffer).status, ev::DecodeStatus::Complete);
    QVERIFY(buffer.isEmpty());
}

void FoundationTests::feeUsesOrderSnapshotAndHalfUpRounding()
{
    const auto result = ev::FeeCalculator::calculate(12.345L, 150, 8000);
    QVERIFY(result.success);
    QCOMPARE(result.data.grossCent, 1852);
    QCOMPARE(result.data.netCent, 1482);
    QCOMPARE(result.data.discountCent, 370);
}

void FoundationTests::feeRejectsInvalidDiscount()
{
    const auto result = ev::FeeCalculator::calculate(1.0L, 100, 0);
    QVERIFY(!result.success);
    QCOMPARE(result.code, ev::ErrorCode::InvalidInput);
}

void FoundationTests::databaseCreatesCoreSchema()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ev::DatabaseManager manager(directory.filePath(QStringLiteral("platform.sqlite3")));
    auto openResult = manager.openForCurrentThread();
    QVERIFY2(openResult.success, qPrintable(openResult.message));
    auto migrationResult = manager.migrate(openResult.data);
    QVERIFY2(migrationResult.success, qPrintable(migrationResult.message));

    const QStringList expectedTables {
        QStringLiteral("users"),
        QStringLiteral("admins"),
        QStringLiteral("charging_stations"),
        QStringLiteral("charging_piles"),
        QStringLiteral("orders")
    };
    const QStringList tables = openResult.data.tables();
    for (const QString &table : expectedTables) {
        QVERIFY2(tables.contains(table), qPrintable(QStringLiteral("missing table: %1").arg(table)));
    }

    QSqlQuery query(openResult.data);
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void FoundationTests::phoneValidation_data()
{
    QTest::addColumn<QString>("phone");
    QTest::addColumn<int>("expectedError");

    QTest::newRow("empty") << QString() << static_cast<int>(ev::PhoneValidationError::Empty);
    QTest::newRow("mobile-13") << QStringLiteral("13800138000")
                                << static_cast<int>(ev::PhoneValidationError::None);
    QTest::newRow("mobile-19") << QStringLiteral("19912345678")
                                << static_cast<int>(ev::PhoneValidationError::None);
    QTest::newRow("invalid-prefix") << QStringLiteral("12800138000")
                                     << static_cast<int>(ev::PhoneValidationError::InvalidFormat);
    QTest::newRow("too-short") << QStringLiteral("1380013800")
                                << static_cast<int>(ev::PhoneValidationError::LengthIncorrect);
    QTest::newRow("too-long") << QStringLiteral("138001380000")
                               << static_cast<int>(ev::PhoneValidationError::LengthIncorrect);
    QTest::newRow("country-code") << QStringLiteral("+8613800138000")
                                   << static_cast<int>(ev::PhoneValidationError::LengthIncorrect);
    QTest::newRow("contains-space") << QStringLiteral("138 00138000")
                                     << static_cast<int>(ev::PhoneValidationError::LengthIncorrect);
    QTest::newRow("contains-letter") << QStringLiteral("1380013800a")
                                      << static_cast<int>(ev::PhoneValidationError::InvalidFormat);
    QTest::newRow("trailing-newline") << QStringLiteral("13800138000\n")
                                       << static_cast<int>(ev::PhoneValidationError::LengthIncorrect);
}

void FoundationTests::phoneValidation()
{
    QFETCH(QString, phone);
    QFETCH(int, expectedError);

    const ev::PhoneValidationResult result = ev::PhoneValidator::validate(QStringView(phone));
    QCOMPARE(static_cast<int>(result.error), expectedError);
}

void FoundationTests::phoneValidationMessages()
{
    QCOMPARE(ev::PhoneValidator::errorMessage(ev::PhoneValidationError::Empty),
             QStringLiteral("请输入手机号"));
    QCOMPARE(ev::PhoneValidator::errorMessage(ev::PhoneValidationError::LengthIncorrect),
             QStringLiteral("请输入11位手机号"));
    QCOMPARE(ev::PhoneValidator::errorMessage(ev::PhoneValidationError::InvalidFormat),
             QStringLiteral("手机号格式不正确，请检查后重新输入"));
}

void FoundationTests::existingUserLoginPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ev::DatabaseManager manager(directory.filePath(QStringLiteral("login.sqlite3")));
    auto openResult = manager.openForCurrentThread();
    QVERIFY2(openResult.success, qPrintable(openResult.message));
    auto migrationResult = manager.migrate(openResult.data);
    QVERIFY2(migrationResult.success, qPrintable(migrationResult.message));

    QSqlQuery insert(openResult.data);
    insert.prepare(QStringLiteral(
        "INSERT INTO users (phone, nickname, avatar_path, balance, status) "
        "VALUES (?, ?, ?, ?, ?)"));
    insert.addBindValue(QStringLiteral("13800138000"));
    insert.addBindValue(QStringLiteral("测试用户"));
    insert.addBindValue(QStringLiteral("/tmp/avatar.png"));
    insert.addBindValue(12.34);
    insert.addBindValue(QStringLiteral("normal"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    insert.prepare(QStringLiteral(
        "INSERT INTO users (phone, nickname, balance, status) VALUES (?, ?, ?, ?)"));
    insert.addBindValue(QStringLiteral("13900139000"));
    insert.addBindValue(QStringLiteral("冻结用户"));
    insert.addBindValue(0.0);
    insert.addBindValue(QStringLiteral("frozen"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    const ev::UserService service;
    const auto success = service.loginExistingUser(openResult.data,
                                                   QStringLiteral("13800138000"));
    QVERIFY2(success.success, qPrintable(success.message));
    QVERIFY(success.data.userId > 0);
    QCOMPARE(success.data.nickname, QStringLiteral("测试用户"));
    QCOMPARE(success.data.balanceCent, 1234);
    QVERIFY(!success.data.sessionId.isEmpty());

    const auto frozen = service.loginExistingUser(openResult.data,
                                                  QStringLiteral("13900139000"));
    QVERIFY(!frozen.success);
    QCOMPARE(frozen.code, ev::ErrorCode::AccountFrozen);
    QCOMPARE(frozen.message, QStringLiteral("账号已被冻结，请联系管理员"));

    const auto missing = service.loginExistingUser(openResult.data,
                                                   QStringLiteral("13700137000"));
    QVERIFY(!missing.success);
    QCOMPARE(missing.code, ev::ErrorCode::NotFound);
    QCOMPARE(missing.message, QStringLiteral("user_not_found"));
}

void FoundationTests::automaticRegistrationCreatesDefaultsAndHandlesConflict()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ev::DatabaseManager manager(directory.filePath(QStringLiteral("register.sqlite3")));
    auto openResult = manager.openForCurrentThread();
    QVERIFY2(openResult.success, qPrintable(openResult.message));
    auto migrationResult = manager.migrate(openResult.data);
    QVERIFY2(migrationResult.success, qPrintable(migrationResult.message));

    const ev::UserService service;
    const auto created = service.registerAutomatically(openResult.data,
                                                       QStringLiteral("13612345678"));
    QVERIFY2(created.success, qPrintable(created.message));
    QVERIFY(created.data.isNewUser);
    QVERIFY(created.data.userId > 0);
    QCOMPARE(created.data.nickname, QStringLiteral("用户5678"));
    QCOMPARE(created.data.avatarPath, QStringLiteral(":/images/default-avatar.svg"));
    QCOMPARE(created.data.balanceCent, 0);
    QVERIFY(!created.data.sessionId.isEmpty());

    QSqlQuery query(openResult.data);
    query.prepare(QStringLiteral(
        "SELECT nickname, avatar_path, balance, status FROM users WHERE phone = ?"));
    query.addBindValue(QStringLiteral("13612345678"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("用户5678"));
    QCOMPARE(query.value(1).toString(), QStringLiteral(":/images/default-avatar.svg"));
    QCOMPARE(query.value(2).toDouble(), 0.0);
    QCOMPARE(query.value(3).toString(), QStringLiteral("normal"));

    const auto conflict = service.registerAutomatically(openResult.data,
                                                        QStringLiteral("13612345678"));
    QVERIFY2(conflict.success, qPrintable(conflict.message));
    QVERIFY(!conflict.data.isNewUser);
    QCOMPARE(conflict.data.userId, created.data.userId);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM users WHERE phone = '13612345678'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void FoundationTests::sessionLifecycle()
{
    ev::SessionManager sessions(5);
    QVERIFY(!sessions.registerUserSession({}, 1));
    QVERIFY(!sessions.registerUserSession(QStringLiteral("invalid-user"), 0));

    QVERIFY(sessions.registerUserSession(QStringLiteral("session-a"), 10));
    QVERIFY(sessions.registerUserSession(QStringLiteral("session-b"), 10));
    QVERIFY(sessions.registerUserSession(QStringLiteral("session-c"), 11));
    QCOMPARE(sessions.activeSessionCount(), 3);
    QVERIFY(sessions.validateAndTouch(QStringLiteral("session-a")));
    QVERIFY(!sessions.validateAndTouch(QStringLiteral("missing")));

    QCOMPARE(sessions.removeByUserId(10), 2);
    QVERIFY(!sessions.validateAndTouch(QStringLiteral("session-a")));
    QCOMPARE(sessions.activeSessionCount(), 1);

    QVERIFY(sessions.registerUserSession(QStringLiteral("expiring"), 12));
    QTest::qWait(10);
    QVERIFY(!sessions.validateAndTouch(QStringLiteral("expiring")));

    QCOMPARE(sessions.removeAll({QStringLiteral("session-c"),
                                 QStringLiteral("does-not-exist")}), 1);
    QCOMPARE(sessions.activeSessionCount(), 0);
}

QTEST_APPLESS_MAIN(FoundationTests)

#include "tst_foundations.moc"
