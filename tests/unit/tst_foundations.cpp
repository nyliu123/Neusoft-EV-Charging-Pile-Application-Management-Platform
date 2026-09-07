#include "common/password_hasher.h"
#include "common/phone_validator.h"
#include "common/protocol.h"
#include "data/database_manager.h"
#include "network/frame_codec.h"
#include "services/admin_auth_service.h"
#include "services/admin_seeder.h"
#include "services/fee_calculator.h"
#include "services/session_manager.h"
#include "services/station_service.h"
#include "services/user_service.h"

#include <QJsonObject>
#include <QJsonArray>
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
    void passwordHashRoundTrip();
    void passwordHashRejectsWrongPassword();
    void passwordSerializationRoundTrip();
    void adminLoginSucceedsWithDefaultSeed();
    void existingUserLoginPaths();
    void automaticRegistrationCreatesDefaultsAndHandlesConflict();
    void userProfileOperationsStayConsistent();
    void stationSearchSortsAndReportsPileStats();
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

void FoundationTests::stationSearchSortsAndReportsPileStats()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ev::DatabaseManager manager(directory.filePath(QStringLiteral("stations.sqlite3")));
    auto openResult = manager.openForCurrentThread();
    QVERIFY2(openResult.success, qPrintable(openResult.message));
    QVERIFY(manager.migrate(openResult.data).success);

    const ev::StationService service;
    const auto stationsResult = service.queryStations(
        openResult.data, true, 121.509605, 38.863650);
    QVERIFY2(stationsResult.success, qPrintable(stationsResult.message));
    const QJsonArray stations = stationsResult.data.value(QStringLiteral("stations")).toArray();
    QCOMPARE(stations.size(), 3);
    QCOMPARE(stations.first().toObject().value(QStringLiteral("station_id")).toInteger(), 1);
    QCOMPARE(stations.first().toObject().value(QStringLiteral("distance_km")).toDouble(), 0.0);
    QCOMPARE(stations.first().toObject().value(QStringLiteral("total_piles")).toInt(), 4);
    QCOMPARE(stations.first().toObject().value(QStringLiteral("idle_count")).toInt(), 2);

    const auto detailResult = service.queryPiles(openResult.data, 1);
    QVERIFY2(detailResult.success, qPrintable(detailResult.message));
    QCOMPARE(detailResult.data.value(QStringLiteral("piles")).toArray().size(), 4);
    const QJsonObject stats = detailResult.data.value(QStringLiteral("stats")).toObject();
    QCOMPARE(stats.value(QStringLiteral("idle")).toInt(), 2);
    QCOMPARE(stats.value(QStringLiteral("in_use")).toInt(), 1);
    QCOMPARE(stats.value(QStringLiteral("fault")).toInt(), 1);
    QCOMPARE(stats.value(QStringLiteral("online_rate")).toDouble(), 75.0);
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

void FoundationTests::passwordHashRoundTrip()
{
    const QString password = QStringLiteral("TestPass123!");
    const ev::PasswordHash hash = ev::PasswordHasher::hash(password);

    QVERIFY(!hash.salt.isEmpty());
    QVERIFY(!hash.hash.isEmpty());
    QVERIFY(hash.iterations > 0);
    QVERIFY(ev::PasswordHasher::verify(password, hash));
}

void FoundationTests::passwordHashRejectsWrongPassword()
{
    const ev::PasswordHash hash = ev::PasswordHasher::hash(QStringLiteral("correct"));
    QVERIFY(!ev::PasswordHasher::verify(QStringLiteral("wrong"), hash));
    QVERIFY(!ev::PasswordHasher::verify(QString(), hash));
}

void FoundationTests::passwordSerializationRoundTrip()
{
    const ev::PasswordHash original = ev::PasswordHasher::hash(QStringLiteral("serializeTest"));
    const QString serialized = ev::PasswordHasher::serialize(original);

    QVERIFY(serialized.startsWith(QStringLiteral("pbkdf2_sha256$")));

    ev::PasswordHash deserialized;
    QVERIFY(ev::PasswordHasher::deserialize(serialized, deserialized));
    QCOMPARE(deserialized.iterations, original.iterations);
    QCOMPARE(deserialized.salt, original.salt);
    QCOMPARE(deserialized.hash, original.hash);
    QVERIFY(ev::PasswordHasher::verify(QStringLiteral("serializeTest"), deserialized));
}

void FoundationTests::adminLoginSucceedsWithDefaultSeed()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + QStringLiteral("/test_login.sqlite3");
    ev::DatabaseManager manager(dbPath);

    auto openResult = manager.openForCurrentThread();
    QVERIFY(openResult.success);

    auto migrateResult = manager.migrate(openResult.data);
    QVERIFY(migrateResult.success);

    // Seed the default admin using AdminSeeder.
    auto seedResult = ev::AdminSeeder::seedIfNeeded(openResult.data);
    QVERIFY(seedResult.success);
    // 002_seed_admin.sql already creates the default account during migration.
    QCOMPARE(seedResult.data, 0);

    // Second seed should be a no-op.
    auto seedResult2 = ev::AdminSeeder::seedIfNeeded(openResult.data);
    QVERIFY(seedResult2.success);
    QCOMPARE(seedResult2.data, 0);

    // Default admin credentials: admin / admin123
    const auto result = ev::AdminAuthService::authenticate(
        QStringLiteral("admin"), QStringLiteral("admin123"), openResult.data);
    QVERIFY(result.success);
    QCOMPARE(result.code, ev::ErrorCode::Ok);
    QCOMPARE(result.data.username, QStringLiteral("admin"));
    QVERIFY(result.data.adminId > 0);

    // Wrong password should fail.
    const auto badResult = ev::AdminAuthService::authenticate(
        QStringLiteral("admin"), QStringLiteral("wrongpass"), openResult.data);
    QVERIFY(!badResult.success);
    QCOMPARE(badResult.code, ev::ErrorCode::Unauthorized);

    // Non-existent user should also return Unauthorized (not NotFound).
    const auto missingResult = ev::AdminAuthService::authenticate(
        QStringLiteral("nonexistent"), QStringLiteral("admin123"), openResult.data);
    QVERIFY(!missingResult.success);
    QCOMPARE(missingResult.code, ev::ErrorCode::Unauthorized);
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
    insert.addBindValue(QStringLiteral("13100138000"));
    insert.addBindValue(QStringLiteral("测试用户"));
    insert.addBindValue(QStringLiteral("/tmp/avatar.png"));
    insert.addBindValue(12.34);
    insert.addBindValue(QStringLiteral("normal"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    insert.prepare(QStringLiteral(
        "INSERT INTO users (phone, nickname, balance, status) VALUES (?, ?, ?, ?)"));
    insert.addBindValue(QStringLiteral("13200139000"));
    insert.addBindValue(QStringLiteral("冻结用户"));
    insert.addBindValue(0.0);
    insert.addBindValue(QStringLiteral("frozen"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    const ev::UserService service;
    const auto success = service.loginExistingUser(openResult.data,
                                                   QStringLiteral("13100138000"));
    QVERIFY2(success.success, qPrintable(success.message));
    QVERIFY(success.data.userId > 0);
    QCOMPARE(success.data.nickname, QStringLiteral("测试用户"));
    QCOMPARE(success.data.balanceCent, 1234);
    QVERIFY(!success.data.sessionId.isEmpty());

    const auto frozen = service.loginExistingUser(openResult.data,
                                                  QStringLiteral("13200139000"));
    QVERIFY(!frozen.success);
    QCOMPARE(frozen.code, ev::ErrorCode::AccountFrozen);
    QCOMPARE(frozen.message, QStringLiteral("账号已被冻结，请联系管理员"));

    const auto missing = service.loginExistingUser(openResult.data,
                                                   QStringLiteral("13300137000"));
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

void FoundationTests::userProfileOperationsStayConsistent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ev::DatabaseManager manager(directory.filePath(QStringLiteral("profile.sqlite3")));
    auto openResult = manager.openForCurrentThread();
    QVERIFY2(openResult.success, qPrintable(openResult.message));
    auto migrationResult = manager.migrate(openResult.data);
    QVERIFY2(migrationResult.success, qPrintable(migrationResult.message));

    const ev::UserService service;
    const auto created = service.registerAutomatically(openResult.data,
                                                       QStringLiteral("13512345678"));
    QVERIFY2(created.success, qPrintable(created.message));
    const qint64 userId = created.data.userId;

    const auto initial = service.queryUserInfo(openResult.data, userId);
    QVERIFY(initial.success);
    QCOMPARE(initial.data.nickname, QStringLiteral("用户5678"));
    QCOMPARE(initial.data.balanceCent, 0);

    const auto invalidNickname = service.updateNickname(
        openResult.data, userId, QStringLiteral("bad nickname"));
    QVERIFY(!invalidNickname.success);
    QCOMPARE(invalidNickname.code, ev::ErrorCode::InvalidInput);

    const auto nickname = service.updateNickname(
        openResult.data, userId, QStringLiteral("新昵称_01"));
    QVERIFY2(nickname.success, qPrintable(nickname.message));
    const auto avatar = service.updateAvatarPath(
        openResult.data, userId, QStringLiteral("/tmp/avatar-new.jpg"));
    QVERIFY2(avatar.success, qPrintable(avatar.message));

    QVERIFY(!service.recharge(openResult.data, userId, 0).success);
    const auto firstRecharge = service.recharge(openResult.data, userId, 1);
    QVERIFY2(firstRecharge.success, qPrintable(firstRecharge.message));
    QCOMPARE(firstRecharge.data, 1);
    const auto secondRecharge = service.recharge(openResult.data, userId, 12345);
    QVERIFY2(secondRecharge.success, qPrintable(secondRecharge.message));
    QCOMPARE(secondRecharge.data, 12346);

    const auto updated = service.queryUserInfo(openResult.data, userId);
    QVERIFY(updated.success);
    QCOMPARE(updated.data.nickname, QStringLiteral("新昵称_01"));
    QCOMPARE(updated.data.avatarPath, QStringLiteral("/tmp/avatar-new.jpg"));
    QCOMPARE(updated.data.balanceCent, 12346);

    QSqlQuery freeze(openResult.data);
    freeze.prepare(QStringLiteral("UPDATE users SET status = 'frozen' WHERE user_id = ?"));
    freeze.addBindValue(userId);
    QVERIFY(freeze.exec());
    const auto frozen = service.queryUserInfo(openResult.data, userId);
    QVERIFY(!frozen.success);
    QCOMPARE(frozen.code, ev::ErrorCode::AccountFrozen);
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
