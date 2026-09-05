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

QTEST_APPLESS_MAIN(FoundationTests)

#include "tst_foundations.moc"
