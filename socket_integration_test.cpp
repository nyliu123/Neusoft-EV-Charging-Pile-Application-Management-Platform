#include "apiclient.h"
#include "chargingserver.h"

#include <QEventLoop>
#include <QCoreApplication>
#include <QEvent>
#include <QHostAddress>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {

struct Response {
    bool received = false;
    bool ok = false;
    QJsonObject data;
    QString code;
    QString message;
};

Response request(evcs::ApiClient &client,
                 const QString &action,
                 const QJsonObject &payload = {})
{
    Response response;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(3000);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QString requestId;
    const QMetaObject::Connection connection = QObject::connect(
        &client, &evcs::ApiClient::responseReceived, &loop,
        [&](const QString &receivedId, const QString &, bool ok, const QJsonObject &data,
            const QString &code, const QString &message) {
            if (receivedId != requestId) return;
            response = {true, ok, data, code, message};
            loop.quit();
        });
    requestId = client.sendRequest(action, payload);
    timer.start();
    loop.exec();
    QObject::disconnect(connection);
    return response;
}

} // namespace

class SocketIntegrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsConcurrentReservationForSameCharger();
    void replaysCompletedDuplicateRequestWithoutRepeatingMutation();
};

void SocketIntegrationTest::rejectsConcurrentReservationForSameCharger()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    evcs::server::ChargingServer server;
    QString error;
    QVERIFY2(server.initialize(temporaryDirectory.filePath(QStringLiteral("socket.db")),
                               QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));

    evcs::ApiClient first;
    evcs::ApiClient second;
    first.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    second.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(first.isConnected(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(second.isConnected(), 3000);

    const Response features = request(first, QStringLiteral("system.features"));
    QVERIFY(features.received && features.ok);
    QVERIFY(!features.data.value(QStringLiteral("loadForecast")).toObject()
                 .value(QStringLiteral("enabled")).toBool(true));
    const Response forecast = request(first, QStringLiteral("analytics.forecast"));
    QVERIFY(forecast.received && !forecast.ok);
    QCOMPARE(forecast.code, QStringLiteral("FEATURE_NOT_READY"));

    for (evcs::ApiClient *client : {&first, &second}) {
        const Response login = request(*client, QStringLiteral("auth.login"), {
            {QStringLiteral("username"), QStringLiteral("demo")},
            {QStringLiteral("password"), QStringLiteral("Demo123!")}
        });
        QVERIFY2(login.received && login.ok, qPrintable(login.message));
        client->setToken(login.data.value(QStringLiteral("token")).toString());
    }

    Response firstReservation;
    Response secondReservation;
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QString firstId;
    QString secondId;
    auto collect = [&](Response &target, const QString &expectedId, const QString &receivedId,
                       bool ok, const QJsonObject &data, const QString &code, const QString &message) {
        if (receivedId != expectedId) return;
        target = {true, ok, data, code, message};
        if (firstReservation.received && secondReservation.received) loop.quit();
    };
    connect(&first, &evcs::ApiClient::responseReceived, &loop,
            [&](const QString &id, const QString &, bool ok, const QJsonObject &data,
                const QString &code, const QString &message) {
        collect(firstReservation, firstId, id, ok, data, code, message);
    });
    connect(&second, &evcs::ApiClient::responseReceived, &loop,
            [&](const QString &id, const QString &, bool ok, const QJsonObject &data,
                const QString &code, const QString &message) {
        collect(secondReservation, secondId, id, ok, data, code, message);
    });
    const QJsonObject payload{{QStringLiteral("chargerId"), 1}};
    firstId = first.sendRequest(QStringLiteral("reservation.create"), payload);
    secondId = second.sendRequest(QStringLiteral("reservation.create"), payload);
    loop.exec();

    QVERIFY(firstReservation.received);
    QVERIFY(secondReservation.received);
    QCOMPARE(static_cast<int>(firstReservation.ok) + static_cast<int>(secondReservation.ok), 1);
    const Response &rejected = firstReservation.ok ? secondReservation : firstReservation;
    QCOMPARE(rejected.code, QStringLiteral("CONFLICT"));

    first.disconnectFromServer();
    second.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!first.isConnected() && !second.isConnected(), 3000);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void SocketIntegrationTest::replaysCompletedDuplicateRequestWithoutRepeatingMutation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    evcs::server::ChargingServer server;
    QString error;
    QVERIFY2(server.initialize(temporaryDirectory.filePath(QStringLiteral("idempotency.db")),
                               QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error), qPrintable(error));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(socket.state(), QAbstractSocket::ConnectedState, 3000);
    evcs::protocol::FrameDecoder decoder;
    QString exchangeError;
    auto exchange = [&](const QJsonObject &requestMessage) {
        socket.write(evcs::protocol::encodeFrame(requestMessage));
        socket.flush();
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.setInterval(3000);
        connect(&socket, &QTcpSocket::readyRead, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start();
        if (socket.bytesAvailable() == 0) loop.exec();
        QString decodeError;
        const QList<QJsonObject> messages = decoder.append(socket.readAll(), &decodeError);
        if (!decodeError.isEmpty() || messages.size() != 1) {
            exchangeError = !decodeError.isEmpty()
                ? decodeError : QStringLiteral("响应帧数量不是 1：%1").arg(messages.size());
            return QJsonObject{};
        }
        return messages.first();
    };

    QJsonObject registration = evcs::protocol::makeRequest(
        QStringLiteral("auth.phoneRegister"),
        {{QStringLiteral("phone"), QStringLiteral("13700001111")}});
    registration.insert(QStringLiteral("requestId"), QStringLiteral("duplicate-registration-1"));
    const QJsonObject firstResponse = exchange(registration);
    const QJsonObject secondResponse = exchange(registration);
    QVERIFY2(exchangeError.isEmpty(), qPrintable(exchangeError));
    QVERIFY(firstResponse.value(QStringLiteral("ok")).toBool());
    QCOMPARE(secondResponse, firstResponse);
    QVERIFY(!firstResponse.value(QStringLiteral("data")).toObject()
                 .value(QStringLiteral("token")).toString().isEmpty());
}

QTEST_MAIN(SocketIntegrationTest)
#include "socket_integration_test.moc"
