#include "common/phone_validator.h"
#include "common/protocol.h"
#include "data/database_manager.h"
#include "network/frame_codec.h"
#include "services/fee_calculator.h"

#include <QJsonObject>
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

QTEST_APPLESS_MAIN(FoundationTests)

#include "tst_foundations.moc"
