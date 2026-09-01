#include "businessservice.h"
#include "database.h"

#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class BusinessFlowTest final : public QObject
{
    Q_OBJECT

private slots:
    void completesReservationChargingAndOrderFlow();
    void administratorCanManageCoreRecords();
    void addedMatrixRequirementsWorkEndToEnd();
    void upgradesLegacyDatabaseSchema();
};

void BusinessFlowTest::completesReservationChargingAndOrderFlow()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    evcs::server::Database database;
    QString error;
    QVERIFY2(database.initialize(
                 temporaryDirectory.filePath(QStringLiteral("flow.db")),
                 QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    evcs::server::BusinessService service(database);

    const auto login = service.handle(
        QStringLiteral("auth.login"),
        {{QStringLiteral("username"), QStringLiteral("demo")},
         {QStringLiteral("password"), QStringLiteral("Demo123!")}}, {});
    QVERIFY2(login.ok, qPrintable(login.errorMessage));
    const QString token = login.data.value(QStringLiteral("token")).toString();
    QVERIFY(!token.isEmpty());

    const auto profile = service.handle(QStringLiteral("user.profile"), {}, token);
    QVERIFY2(profile.ok, qPrintable(profile.errorMessage));
    QCOMPARE(profile.data.value(QStringLiteral("user")).toObject()
                 .value(QStringLiteral("username")).toString(), QStringLiteral("demo"));

    const auto stationList = service.handle(QStringLiteral("station.list"), {}, token);
    QVERIFY2(stationList.ok, qPrintable(stationList.errorMessage));
    const QJsonArray stations = stationList.data.value(QStringLiteral("stations")).toArray();
    QCOMPARE(stations.size(), 3);

    const auto station = service.handle(
        QStringLiteral("station.get"),
        {{QStringLiteral("stationId"), stations.first().toObject().value(QStringLiteral("id"))}},
        token);
    QVERIFY2(station.ok, qPrintable(station.errorMessage));
    const QJsonArray chargers = station.data.value(QStringLiteral("station")).toObject()
                                    .value(QStringLiteral("chargers")).toArray();
    QVERIFY(!chargers.isEmpty());
    const double chargerId = chargers.first().toObject().value(QStringLiteral("id")).toDouble();

    const auto reservation = service.handle(
        QStringLiteral("reservation.create"),
        {{QStringLiteral("chargerId"), chargerId}}, token);
    QVERIFY2(reservation.ok, qPrintable(reservation.errorMessage));
    const double reservationId = reservation.data.value(QStringLiteral("reservationId")).toDouble();

    const auto duplicateReservation = service.handle(
        QStringLiteral("reservation.create"),
        {{QStringLiteral("chargerId"), chargerId}}, token);
    QVERIFY(!duplicateReservation.ok);
    QCOMPARE(duplicateReservation.errorCode, QStringLiteral("CONFLICT"));

    const auto session = service.handle(
        QStringLiteral("charging.start"),
        {{QStringLiteral("reservationId"), reservationId}}, token);
    QVERIFY2(session.ok, qPrintable(session.errorMessage));
    const double sessionId = session.data.value(QStringLiteral("sessionId")).toDouble();

    const auto status = service.handle(
        QStringLiteral("charging.status"),
        {{QStringLiteral("sessionId"), sessionId}}, token);
    QVERIFY2(status.ok, qPrintable(status.errorMessage));
    QCOMPARE(status.data.value(QStringLiteral("session")).toObject()
                 .value(QStringLiteral("status")).toString(),
             QStringLiteral("charging"));

    const auto stopped = service.handle(
        QStringLiteral("charging.stop"),
        {{QStringLiteral("sessionId"), sessionId}}, token);
    QVERIFY2(stopped.ok, qPrintable(stopped.errorMessage));
    const QJsonObject order = stopped.data.value(QStringLiteral("order")).toObject();
    QVERIFY(!order.value(QStringLiteral("orderNo")).toString().isEmpty());

    const auto orders = service.handle(QStringLiteral("order.list"), {}, token);
    QVERIFY2(orders.ok, qPrintable(orders.errorMessage));
    QCOMPARE(orders.data.value(QStringLiteral("orders")).toArray().size(), 1);

    const auto logout = service.handle(QStringLiteral("auth.logout"), {}, token);
    QVERIFY2(logout.ok, qPrintable(logout.errorMessage));
    const auto afterLogout = service.handle(QStringLiteral("order.list"), {}, token);
    QVERIFY(!afterLogout.ok);
    QCOMPARE(afterLogout.errorCode, QStringLiteral("UNAUTHENTICATED"));
}

void BusinessFlowTest::administratorCanManageCoreRecords()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    evcs::server::Database database;
    QString error;
    QVERIFY2(database.initialize(
                 temporaryDirectory.filePath(QStringLiteral("admin.db")),
                 QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    evcs::server::BusinessService service(database);

    const auto login = service.handle(
        QStringLiteral("auth.login"),
        {{QStringLiteral("username"), QStringLiteral("admin")},
         {QStringLiteral("password"), QStringLiteral("Admin123!")}}, {});
    QVERIFY2(login.ok, qPrintable(login.errorMessage));
    const QString token = login.data.value(QStringLiteral("token")).toString();

    const auto dashboard = service.handle(QStringLiteral("admin.dashboard"), {}, token);
    QVERIFY2(dashboard.ok, qPrintable(dashboard.errorMessage));
    QCOMPARE(dashboard.data.value(QStringLiteral("stationCount")).toInt(), 3);
    QCOMPARE(dashboard.data.value(QStringLiteral("chargerCount")).toInt(), 6);
    QCOMPARE(dashboard.data.value(QStringLiteral("sevenDayTrend")).toArray().size(), 7);
    QCOMPARE(dashboard.data.value(QStringLiteral("thirtyDayTrend")).toArray().size(), 30);

    const auto station = service.handle(
        QStringLiteral("admin.station.save"),
        {{QStringLiteral("name"), QStringLiteral("测试充电站")},
         {QStringLiteral("region"), QStringLiteral("测试区")},
         {QStringLiteral("address"), QStringLiteral("测试路 1 号")},
         {QStringLiteral("longitude"), 116.1},
         {QStringLiteral("latitude"), 39.9},
         {QStringLiteral("status"), QStringLiteral("active")}}, token);
    QVERIFY2(station.ok, qPrintable(station.errorMessage));
    const double stationId = station.data.value(QStringLiteral("stationId")).toDouble();

    const auto tariff = service.handle(
        QStringLiteral("admin.tariff.save"),
        {{QStringLiteral("name"), QStringLiteral("测试价格")},
         {QStringLiteral("priceCentsPerKwh"), 95},
         {QStringLiteral("active"), true}}, token);
    QVERIFY2(tariff.ok, qPrintable(tariff.errorMessage));
    const double tariffId = tariff.data.value(QStringLiteral("tariffId")).toDouble();

    const auto charger = service.handle(
        QStringLiteral("admin.charger.save"),
        {{QStringLiteral("stationId"), stationId},
         {QStringLiteral("tariffId"), tariffId},
         {QStringLiteral("code"), QStringLiteral("TEST-001")},
         {QStringLiteral("connectorType"), QStringLiteral("GB/T")},
         {QStringLiteral("ratedPowerKw"), 22.0},
         {QStringLiteral("status"), QStringLiteral("idle")}}, token);
    QVERIFY2(charger.ok, qPrintable(charger.errorMessage));
    const double chargerId = charger.data.value(QStringLiteral("chargerId")).toDouble();

    const auto fault = service.handle(
        QStringLiteral("admin.fault.save"),
        {{QStringLiteral("chargerId"), chargerId},
         {QStringLiteral("title"), QStringLiteral("测试故障")},
         {QStringLiteral("description"), QStringLiteral("测试故障描述")},
         {QStringLiteral("status"), QStringLiteral("open")}}, token);
    QVERIFY2(fault.ok, qPrintable(fault.errorMessage));

    const auto chargers = service.handle(QStringLiteral("admin.charger.list"), {}, token);
    QVERIFY2(chargers.ok, qPrintable(chargers.errorMessage));
    const auto chargerRows = chargers.data.value(QStringLiteral("chargers")).toArray();
    bool foundFaultCharger = false;
    for (const QJsonValue &value : chargerRows) {
        const QJsonObject row = value.toObject();
        if (row.value(QStringLiteral("id")).toDouble() == chargerId) {
            foundFaultCharger = row.value(QStringLiteral("status")).toString()
                == QStringLiteral("fault");
        }
    }
    QVERIFY(foundFaultCharger);

    const auto users = service.handle(QStringLiteral("admin.user.list"), {}, token);
    QVERIFY2(users.ok, qPrintable(users.errorMessage));
    QVERIFY(users.data.value(QStringLiteral("users")).toArray().size() >= 2);

    const auto faults = service.handle(QStringLiteral("admin.fault.list"), {}, token);
    QVERIFY2(faults.ok, qPrintable(faults.errorMessage));
    QCOMPARE(faults.data.value(QStringLiteral("faults")).toArray().size(), 1);

    const auto reservations = service.handle(QStringLiteral("admin.reservation.list"), {}, token);
    QVERIFY2(reservations.ok, qPrintable(reservations.errorMessage));
    const auto sessions = service.handle(QStringLiteral("admin.session.list"), {}, token);
    QVERIFY2(sessions.ok, qPrintable(sessions.errorMessage));

    const auto unconfirmed = service.handle(QStringLiteral("admin.demo.generateHistory"), {}, token);
    QVERIFY(!unconfirmed.ok);
    QCOMPARE(unconfirmed.errorCode, QStringLiteral("CONFIRMATION_REQUIRED"));
    const auto generated = service.handle(
        QStringLiteral("admin.demo.generateHistory"),
        {{QStringLiteral("confirmed"), true}}, token);
    QVERIFY2(generated.ok, qPrintable(generated.errorMessage));
    QVERIFY(generated.data.value(QStringLiteral("insertedOrders")).toInt() >= 60);
    const auto generatedAgain = service.handle(
        QStringLiteral("admin.demo.generateHistory"),
        {{QStringLiteral("confirmed"), true}}, token);
    QVERIFY2(generatedAgain.ok, qPrintable(generatedAgain.errorMessage));
    QCOMPARE(generatedAgain.data.value(QStringLiteral("insertedOrders")).toInt(), 0);

    const auto analytics = service.handle(QStringLiteral("admin.analytics"), {}, token);
    QVERIFY2(analytics.ok, qPrintable(analytics.errorMessage));
    QCOMPARE(analytics.data.value(QStringLiteral("dailyTrend")).toArray().size(), 30);
    QVERIFY(analytics.data.value(QStringLiteral("summary")).toObject()
                .value(QStringLiteral("orderCount")).toInt() >= 60);
}

void BusinessFlowTest::addedMatrixRequirementsWorkEndToEnd()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    evcs::server::Database database;
    QString error;
    QVERIFY2(database.initialize(
                 temporaryDirectory.filePath(QStringLiteral("matrix-gaps.db")),
                 QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    evcs::server::BusinessService service(database);

    const auto invalidPhone = service.handle(
        QStringLiteral("auth.phoneLogin"),
        {{QStringLiteral("phone"), QStringLiteral("123456")}}, {});
    QVERIFY(!invalidPhone.ok);
    QCOMPARE(invalidPhone.errorCode, QStringLiteral("INVALID_ARGUMENT"));

    const QString phone = QStringLiteral("13912345678");
    const auto firstPhoneLogin = service.handle(
        QStringLiteral("auth.phoneLogin"), {{QStringLiteral("phone"), phone}}, {});
    QVERIFY2(firstPhoneLogin.ok, qPrintable(firstPhoneLogin.errorMessage));
    QVERIFY(firstPhoneLogin.data.value(QStringLiteral("autoRegistered")).toBool());
    const QString userToken = firstPhoneLogin.data.value(QStringLiteral("token")).toString();
    QVERIFY(!userToken.isEmpty());
    const auto secondPhoneLogin = service.handle(
        QStringLiteral("auth.phoneLogin"), {{QStringLiteral("phone"), phone}}, {});
    QVERIFY2(secondPhoneLogin.ok, qPrintable(secondPhoneLogin.errorMessage));
    QVERIFY(!secondPhoneLogin.data.value(QStringLiteral("autoRegistered")).toBool(true));

    const auto renamed = service.handle(
        QStringLiteral("user.profile.update"),
        {{QStringLiteral("displayName"), QStringLiteral("矩阵测试用户")}}, userToken);
    QVERIFY2(renamed.ok, qPrintable(renamed.errorMessage));
    const auto avatar = service.handle(
        QStringLiteral("user.avatar.update"),
        {{QStringLiteral("mimeType"), QStringLiteral("image/png")},
         {QStringLiteral("dataBase64"), QStringLiteral("iVBORw0KGgo=")}}, userToken);
    QVERIFY2(avatar.ok, qPrintable(avatar.errorMessage));
    const auto recharge = service.handle(
        QStringLiteral("wallet.recharge"),
        {{QStringLiteral("amountCents"), 1234}}, userToken);
    QVERIFY2(recharge.ok, qPrintable(recharge.errorMessage));
    QCOMPARE(recharge.data.value(QStringLiteral("balanceCents")).toInt(), 11234);
    const auto profile = service.handle(QStringLiteral("user.profile"), {}, userToken);
    QVERIFY2(profile.ok, qPrintable(profile.errorMessage));
    const QJsonObject user = profile.data.value(QStringLiteral("user")).toObject();
    QCOMPARE(user.value(QStringLiteral("displayName")).toString(), QStringLiteral("矩阵测试用户"));
    QCOMPARE(user.value(QStringLiteral("avatarMime")).toString(), QStringLiteral("image/png"));
    QVERIFY(!user.value(QStringLiteral("avatarBase64")).toString().isEmpty());

    const auto nearby = service.handle(
        QStringLiteral("station.list"),
        {{QStringLiteral("latitude"), 39.908}, {QStringLiteral("longitude"), 116.397}},
        userToken);
    QVERIFY2(nearby.ok, qPrintable(nearby.errorMessage));
    const QJsonArray nearbyStations = nearby.data.value(QStringLiteral("stations")).toArray();
    QVERIFY(nearbyStations.size() >= 3);
    double lastDistance = -1.0;
    for (const QJsonValue &value : nearbyStations) {
        const double distance = value.toObject().value(QStringLiteral("distanceKm")).toDouble(-1.0);
        QVERIFY(distance >= 0.0);
        QVERIFY(distance >= lastDistance);
        lastDistance = distance;
    }

    const auto adminLogin = service.handle(
        QStringLiteral("auth.login"),
        {{QStringLiteral("username"), QStringLiteral("admin")},
         {QStringLiteral("password"), QStringLiteral("Admin123!")}}, {});
    QVERIFY2(adminLogin.ok, qPrintable(adminLogin.errorMessage));
    const QString adminToken = adminLogin.data.value(QStringLiteral("token")).toString();

    const auto filteredUsers = service.handle(
        QStringLiteral("admin.user.list"),
        {{QStringLiteral("phoneKeyword"), QStringLiteral("91234")}}, adminToken);
    QVERIFY2(filteredUsers.ok, qPrintable(filteredUsers.errorMessage));
    const QJsonArray filteredUserRows = filteredUsers.data.value(QStringLiteral("users")).toArray();
    QCOMPARE(filteredUserRows.size(), 1);
    QCOMPARE(filteredUserRows.first().toObject().value(QStringLiteral("phone")).toString(), phone);

    const auto station = service.handle(
        QStringLiteral("admin.station.save"),
        {{QStringLiteral("name"), QStringLiteral("联动测试站")},
         {QStringLiteral("region"), QStringLiteral("测试区")},
         {QStringLiteral("address"), QStringLiteral("联动路 1 号")},
         {QStringLiteral("longitude"), 116.4},
         {QStringLiteral("latitude"), 39.9},
         {QStringLiteral("chargerCount"), 2},
         {QStringLiteral("defaultPowerKw"), 120.0},
         {QStringLiteral("tariffId"), 1},
         {QStringLiteral("status"), QStringLiteral("active")}}, adminToken);
    QVERIFY2(station.ok, qPrintable(station.errorMessage));
    QCOMPARE(station.data.value(QStringLiteral("createdChargers")).toInt(), 2);
    const double stationId = station.data.value(QStringLiteral("stationId")).toDouble();
    const auto linkedChargers = service.handle(
        QStringLiteral("admin.charger.list"),
        {{QStringLiteral("stationId"), stationId}}, adminToken);
    QVERIFY2(linkedChargers.ok, qPrintable(linkedChargers.errorMessage));
    const QJsonArray chargerRows = linkedChargers.data.value(QStringLiteral("chargers")).toArray();
    QCOMPARE(chargerRows.size(), 2);
    QVERIFY(chargerRows.first().toObject().contains(QStringLiteral("sessionCount")));
    QVERIFY(chargerRows.first().toObject().contains(QStringLiteral("totalDurationSeconds")));
    const double chargerId = chargerRows.first().toObject().value(QStringLiteral("id")).toDouble();

    const auto faulted = service.handle(
        QStringLiteral("admin.charger.setStatus"),
        {{QStringLiteral("chargerId"), chargerId}, {QStringLiteral("status"), QStringLiteral("fault")}},
        adminToken);
    QVERIFY2(faulted.ok, qPrintable(faulted.errorMessage));
    const auto restarted = service.handle(
        QStringLiteral("admin.charger.restart"),
        {{QStringLiteral("chargerId"), chargerId}}, adminToken);
    QVERIFY2(restarted.ok, qPrintable(restarted.errorMessage));
    QVERIFY(restarted.data.value(QStringLiteral("success")).toBool());
    QCOMPARE(restarted.data.value(QStringLiteral("resultStatus")).toString(), QStringLiteral("idle"));
    const auto operationLogs = service.handle(
        QStringLiteral("admin.charger.operation.list"),
        {{QStringLiteral("chargerId"), chargerId}}, adminToken);
    QVERIFY2(operationLogs.ok, qPrintable(operationLogs.errorMessage));
    QCOMPARE(operationLogs.data.value(QStringLiteral("operations")).toArray().size(), 1);

    const auto generated = service.handle(
        QStringLiteral("admin.demo.generateHistory"),
        {{QStringLiteral("confirmed"), true}}, adminToken);
    QVERIFY2(generated.ok, qPrintable(generated.errorMessage));
    const auto allOrders = service.handle(QStringLiteral("admin.order.list"), {}, adminToken);
    QVERIFY2(allOrders.ok, qPrintable(allOrders.errorMessage));
    const QJsonObject sampleOrder = allOrders.data.value(QStringLiteral("orders")).toArray().first().toObject();
    const QString orderNeedle = sampleOrder.value(QStringLiteral("orderNo")).toString().right(8);
    const auto filteredOrders = service.handle(
        QStringLiteral("admin.order.list"),
        {{QStringLiteral("orderNo"), orderNeedle},
         {QStringLiteral("stationKeyword"), sampleOrder.value(QStringLiteral("stationName"))}},
        adminToken);
    QVERIFY2(filteredOrders.ok, qPrintable(filteredOrders.errorMessage));
    QVERIFY(!filteredOrders.data.value(QStringLiteral("orders")).toArray().isEmpty());
}

void BusinessFlowTest::upgradesLegacyDatabaseSchema()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(QStringLiteral("legacy.db"));
    const QString connectionName = QStringLiteral("legacy-schema-setup");
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        legacy.setDatabaseName(databasePath);
        QVERIFY(legacy.open());
        QSqlQuery query(legacy);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE users(id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE, "
            "password_hash TEXT NOT NULL, password_salt TEXT NOT NULL, role TEXT NOT NULL, "
            "display_name TEXT NOT NULL, phone TEXT NOT NULL DEFAULT '', balance_cents INTEGER NOT NULL, "
            "status TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE schema_version(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO schema_version(version, applied_at) VALUES(1, '2026-08-31T00:00:00Z')")));
        legacy.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    evcs::server::Database database;
    QString error;
    QVERIFY2(database.initialize(databasePath, QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    QSqlQuery columns(database.connection());
    QVERIFY(columns.exec(QStringLiteral("PRAGMA table_info(users)")));
    bool foundAvatarMime = false;
    bool foundAvatarData = false;
    while (columns.next()) {
        foundAvatarMime |= columns.value(1).toString() == QStringLiteral("avatar_mime");
        foundAvatarData |= columns.value(1).toString() == QStringLiteral("avatar_data");
    }
    QVERIFY(foundAvatarMime);
    QVERIFY(foundAvatarData);
    QSqlQuery version(database.connection());
    QVERIFY(version.exec(QStringLiteral("SELECT COUNT(*) FROM schema_version WHERE version = 2")));
    QVERIFY(version.next());
    QCOMPARE(version.value(0).toInt(), 1);
}

QTEST_APPLESS_MAIN(BusinessFlowTest)

#include "business_flow_test.moc"
