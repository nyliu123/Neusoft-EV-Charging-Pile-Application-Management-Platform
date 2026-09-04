#include "chargingserver.h"

#include "businessservice.h"

#include <QtConcurrent>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QPointer>
#include <QThread>
#include <QTcpSocket>

#include <exception>

namespace evcs::server {

ChargingServer::ChargingServer(QObject *parent)
    : QObject(parent)
{
    // 限制线程池规模，兼顾多客户端并发能力与教学虚拟机资源占用。
    workerPool_.setMaxThreadCount(qBound(2, QThread::idealThreadCount(), 8));
    workerPool_.setExpiryTimeout(30000);
    connect(&server_, &QTcpServer::newConnection,
            this, &ChargingServer::acceptPendingConnections);
    chargingPushTimer_.setInterval(1000);
    connect(&chargingPushTimer_, &QTimer::timeout, this, &ChargingServer::pushChargingUpdates);
    chargingPushTimer_.start();
}

ChargingServer::~ChargingServer()
{
    server_.close();
    const QList<QTcpSocket *> sockets = decoders_.keys();
    for (QTcpSocket *socket : sockets) {
        QObject::disconnect(socket, nullptr, this, nullptr);
        socket->disconnectFromHost();
    }
    decoders_.clear();
    workerPool_.waitForDone();
}

bool ChargingServer::initialize(const QString &databasePath,
                                const QString &schemaPath,
                                QString *errorMessage)
{
    if (!database_.initialize(databasePath, schemaPath, errorMessage)) return false;
    databasePath_ = databasePath;
    qInfo().noquote() << QStringLiteral("worker_pool_ready threads=%1 database=%2")
                            .arg(workerPool_.maxThreadCount()).arg(databasePath_);
    return true;
}

bool ChargingServer::listen(const QHostAddress &address,
                            quint16 port,
                            QString *errorMessage)
{
    if (server_.listen(address, port)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = server_.errorString();
    }
    return false;
}

quint16 ChargingServer::serverPort() const
{
    return server_.serverPort();
}

void ChargingServer::configureMap(const QString &apiKey, const QString &referer)
{
    mapApi_.configure(apiKey, referer);
}

void ChargingServer::acceptPendingConnections()
{
    while (server_.hasPendingConnections()) {
        QTcpSocket *socket = server_.nextPendingConnection();
        decoders_.insert(socket, protocol::FrameDecoder{});
        qInfo().noquote() << QStringLiteral("client_connected peer=%1:%2")
                                .arg(socket->peerAddress().toString())
                                .arg(socket->peerPort());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            readClient(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            removeClient(socket);
        });
    }
}

void ChargingServer::readClient(QTcpSocket *socket)
{
    // TCP 可能拆包或粘包，先交给帧解码器拼成完整 JSON 请求。
    QString errorMessage;
    const auto messages = decoders_[socket].append(socket->readAll(), &errorMessage);
    if (!errorMessage.isEmpty()) {
        sendMessage(socket, protocol::makeErrorResponse(
                                {}, QStringLiteral("INVALID_MESSAGE"), errorMessage));
        socket->disconnectFromHost();
        return;
    }

    for (const QJsonObject &message : messages) {
        dispatchMessage(socket, message);
    }
}

void ChargingServer::removeClient(QTcpSocket *socket)
{
    qInfo().noquote() << QStringLiteral("client_disconnected peer=%1:%2")
                            .arg(socket->peerAddress().toString())
                            .arg(socket->peerPort());
    decoders_.remove(socket);
    chargingSubscribers_.remove(socket);
    chargingPushInFlight_.remove(socket);
    socket->deleteLater();
}

void ChargingServer::sendMessage(QTcpSocket *socket, const QJsonObject &message)
{
    socket->write(protocol::encodeFrame(message));
}

void ChargingServer::dispatchMessage(QTcpSocket *socket, const QJsonObject &message)
{
    // 主线程完成协议校验，耗时数据库业务放入线程池异步执行。
    const QString requestId = message.value(QStringLiteral("requestId")).toString();
    const QString action = message.value(QStringLiteral("action")).toString();
    if (message.value(QStringLiteral("type")).toString() != QStringLiteral("request")
        || requestId.isEmpty() || action.isEmpty()) {
        sendMessage(socket, protocol::makeErrorResponse(requestId,
                    QStringLiteral("INVALID_MESSAGE"),
                    QStringLiteral("请求缺少 type、requestId 或 action")));
        return;
    }
    const QJsonValue payloadValue = message.value(QStringLiteral("payload"));
    if (!payloadValue.isUndefined() && !payloadValue.isObject()) {
        sendMessage(socket, protocol::makeErrorResponse(requestId,
                    QStringLiteral("INVALID_ARGUMENT"),
                    QStringLiteral("payload 必须是 JSON 对象")));
        return;
    }
    const QString cacheKey = message.value(QStringLiteral("token")).toString()
        + QLatin1Char('|') + requestId;
    if (responseCache_.contains(cacheKey)) {
        sendMessage(socket, responseCache_.value(cacheKey));
        return;
    }
    if (inFlightRequests_.contains(cacheKey)) {
        sendMessage(socket, protocol::makeErrorResponse(
            requestId, QStringLiteral("DUPLICATE_REQUEST"), QStringLiteral("相同请求正在处理中")));
        return;
    }
    inFlightRequests_.insert(cacheKey);
    if (action == QStringLiteral("map.geocode")) {
        const QString address = payloadValue.toObject().value(QStringLiteral("address")).toString();
        QPointer<QTcpSocket> socketGuard(socket);
        mapApi_.geocode(address, [this, socketGuard, requestId, cacheKey](bool ok, const QJsonObject &data,
                                                               const QString &error) {
            const QJsonObject response = ok
                ? protocol::makeSuccessResponse(requestId, data)
                : protocol::makeErrorResponse(requestId, QStringLiteral("GEOCODE_FAILED"), error);
            inFlightRequests_.remove(cacheKey);
            responseCache_.insert(cacheKey, response);
            if (!socketGuard || socketGuard->state() != QAbstractSocket::ConnectedState) return;
            sendMessage(socketGuard.data(), response);
        });
        return;
    }

    QPointer<QTcpSocket> socketGuard(socket);
    [[maybe_unused]] const auto worker = QtConcurrent::run(
        &workerPool_, [this, socketGuard, message, requestId, action, cacheKey] {
        qInfo().noquote() << QStringLiteral("request_started requestId=%1 action=%2 thread=%3")
                                .arg(requestId, action)
                                .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        const QJsonObject response = handleMessage(message);
        const QString token = message.value(QStringLiteral("token")).toString();
        QMetaObject::invokeMethod(this, [this, socketGuard, response, requestId, action, token, cacheKey] {
            inFlightRequests_.remove(cacheKey);
            responseCache_.insert(cacheKey, response);
            while (responseCache_.size() > 500) responseCache_.erase(responseCache_.begin());
            if (socketGuard && socketGuard->state() == QAbstractSocket::ConnectedState
                && decoders_.contains(socketGuard.data())) {
                sendMessage(socketGuard.data(), response);
                const bool ok = response.value(QStringLiteral("ok")).toBool(false);
                if (ok && (action == QStringLiteral("charging.start")
                           || action == QStringLiteral("charging.status"))) {
                    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
                    const QJsonObject session = data.value(QStringLiteral("session")).toObject();
                    const QString status = session.isEmpty()
                        ? data.value(QStringLiteral("status")).toString()
                        : session.value(QStringLiteral("status")).toString();
                    if (status == QStringLiteral("charging")) chargingSubscribers_[socketGuard.data()] = token;
                }
                if (action == QStringLiteral("charging.stop") || action == QStringLiteral("auth.logout")) {
                    chargingSubscribers_.remove(socketGuard.data());
                }
            }
            qInfo().noquote() << QStringLiteral("request_finished requestId=%1 action=%2")
                                    .arg(requestId, action);
        }, Qt::QueuedConnection);
        });
}

void ChargingServer::pushChargingUpdates()
{
    const QList<QTcpSocket *> sockets = chargingSubscribers_.keys();
    for (QTcpSocket *socket : sockets) {
        if (!socket || socket->state() != QAbstractSocket::ConnectedState
            || chargingPushInFlight_.contains(socket)) continue;
        const QString token = chargingSubscribers_.value(socket);
        chargingPushInFlight_.insert(socket);
        QPointer<QTcpSocket> socketGuard(socket);
        [[maybe_unused]] const auto worker = QtConcurrent::run(&workerPool_, [this, socketGuard, token] {
            const QJsonObject request{
                {QStringLiteral("type"), QStringLiteral("request")},
                {QStringLiteral("requestId"), QStringLiteral("charging-push")},
                {QStringLiteral("action"), QStringLiteral("charging.status")},
                {QStringLiteral("payload"), QJsonObject{}},
                {QStringLiteral("token"), token}
            };
            const QJsonObject response = handleMessage(request);
            QMetaObject::invokeMethod(this, [this, socketGuard, response] {
                if (socketGuard) chargingPushInFlight_.remove(socketGuard.data());
                if (!socketGuard || socketGuard->state() != QAbstractSocket::ConnectedState) return;
                if (!response.value(QStringLiteral("ok")).toBool(false)) {
                    chargingSubscribers_.remove(socketGuard.data());
                    return;
                }
                sendMessage(socketGuard.data(), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("event")},
                    {QStringLiteral("action"), QStringLiteral("charging.update")},
                    {QStringLiteral("data"), response.value(QStringLiteral("data")).toObject()}
                });
            }, Qt::QueuedConnection);
        });
    }
}

QJsonObject ChargingServer::handleMessage(const QJsonObject &message) const
{
    // 每个工作线程建立独立 SQLite 连接，避免跨线程复用数据库对象。
    const QString requestId = message.value(QStringLiteral("requestId")).toString();
    const QString action = message.value(QStringLiteral("action")).toString();
    if (action == QStringLiteral("system.ping")) {
        return protocol::makeSuccessResponse(requestId, {
            {QStringLiteral("service"), QStringLiteral("evcs_server")},
            {QStringLiteral("version"), QStringLiteral("1.2.0")},
            {QStringLiteral("serverTime"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
        });
    }

    if (action == QStringLiteral("system.features")) {
        return protocol::makeSuccessResponse(requestId, {
            {QStringLiteral("coreCharging"), QJsonObject{{QStringLiteral("enabled"), true}}},
            {QStringLiteral("operationsDashboard"), QJsonObject{{QStringLiteral("enabled"), true}}},
            {QStringLiteral("loadForecast"), QJsonObject{
                 {QStringLiteral("enabled"), false},
                 {QStringLiteral("reason"), QStringLiteral("缺少经验证的连续历史负荷与天气数据")}
             }},
            {QStringLiteral("smartRecommendation"), QJsonObject{
                 {QStringLiteral("enabled"), false},
                 {QStringLiteral("reason"), QStringLiteral("缺少足够的匿名用户选择与到站结果数据")}
             }}
        });
    }

    if (action == QStringLiteral("analytics.forecast")
        || action == QStringLiteral("recommendation.list")) {
        return protocol::makeErrorResponse(requestId,
                                           QStringLiteral("FEATURE_NOT_READY"),
                                           QStringLiteral("该数据功能将在核心系统完成后启用"));
    }

    const QJsonValue payloadValue = message.value(QStringLiteral("payload"));
    try {
        Database workerDatabase;
        QString databaseError;
        if (!workerDatabase.openExisting(databasePath_, &databaseError)) {
            return protocol::makeErrorResponse(requestId, QStringLiteral("DATABASE_ERROR"), databaseError);
        }
        BusinessService businessService(workerDatabase);
        const ServiceResult result = businessService.handle(
            action,
            payloadValue.isObject() ? payloadValue.toObject() : QJsonObject{},
            message.value(QStringLiteral("token")).toString());
        if (!result.ok) {
            qWarning().noquote() << QStringLiteral("request_failed requestId=%1 action=%2 code=%3")
                                        .arg(requestId, action, result.errorCode);
        }
        return result.ok
            ? protocol::makeSuccessResponse(requestId, result.data)
            : protocol::makeErrorResponse(requestId, result.errorCode, result.errorMessage);
    } catch (const std::exception &exception) {
        qCritical().noquote() << QStringLiteral("request_exception requestId=%1 action=%2 detail=%3")
                                     .arg(requestId, action, QString::fromUtf8(exception.what()));
    } catch (...) {
        qCritical().noquote() << QStringLiteral("request_exception requestId=%1 action=%2 detail=unknown")
                                     .arg(requestId, action);
    }
    return protocol::makeErrorResponse(requestId,
                                       QStringLiteral("INTERNAL_ERROR"),
                                       QStringLiteral("服务端处理请求时发生异常"));
}

} // namespace evcs::server
