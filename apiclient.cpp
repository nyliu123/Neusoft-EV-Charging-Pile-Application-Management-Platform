#include "apiclient.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QStringList>

namespace evcs {

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
    reconnectTimer_.setSingleShot(true);
    reconnectTimer_.setInterval(2000);
    connect(&reconnectTimer_, &QTimer::timeout, this, [this] {
        if (reconnectEnabled_ && socket_.state() == QAbstractSocket::UnconnectedState) {
            emit connectionChanged(false, QStringLiteral("正在自动重连 %1:%2…").arg(host_).arg(port_));
            socket_.connectToHost(host_, port_);
        }
    });
    requestTimer_.setInterval(1000);
    requestTimer_.start();
    connect(&requestTimer_, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QStringList ids = requestDeadlines_.keys();
        for (const QString &requestId : ids) {
            if (requestDeadlines_.value(requestId) > now) continue;
            requestDeadlines_.remove(requestId);
            const QString action = pendingActions_.take(requestId);
            emit responseReceived(requestId, action, false, {},
                                  QStringLiteral("REQUEST_TIMEOUT"),
                                  QStringLiteral("服务端响应超时，请检查网络后重试"));
        }
    });
    connect(&socket_, &QTcpSocket::connected, this, [this] {
        reconnectTimer_.stop();
        emit connectionChanged(true, QStringLiteral("已连接服务端"));
    });
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        decoder_.reset();
        pendingActions_.clear();
        requestDeadlines_.clear();
        emit connectionChanged(false, QStringLiteral("服务端连接已断开"));
        scheduleReconnect();
    });
    connect(&socket_, &QTcpSocket::readyRead, this, &ApiClient::readResponses);
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        emit connectionChanged(false, socket_.errorString());
        scheduleReconnect();
    });
}

ApiClient::~ApiClient()
{
    reconnectEnabled_ = false;
    reconnectTimer_.stop();
    QObject::disconnect(&socket_, nullptr, this, nullptr);
    socket_.abort();
}

void ApiClient::connectToServer(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;
    reconnectEnabled_ = true;
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.abort();
    }
    emit connectionChanged(false, QStringLiteral("正在连接 %1:%2…").arg(host).arg(port));
    socket_.connectToHost(host, port);
}

void ApiClient::disconnectFromServer()
{
    reconnectEnabled_ = false;
    reconnectTimer_.stop();
    socket_.disconnectFromHost();
}

void ApiClient::scheduleReconnect()
{
    if (reconnectEnabled_ && !host_.isEmpty() && port_ > 0 && !reconnectTimer_.isActive()) {
        reconnectTimer_.start();
    }
}

bool ApiClient::isConnected() const
{
    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString ApiClient::token() const
{
    return token_;
}

void ApiClient::setToken(const QString &token)
{
    token_ = token;
}

void ApiClient::clearToken()
{
    token_.clear();
}

QString ApiClient::sendRequest(const QString &action, const QJsonObject &payload)
{
    if (!isConnected()) {
        return {};
    }
    const QJsonObject request = protocol::makeRequest(action, payload, token_);
    const QString requestId = request.value(QStringLiteral("requestId")).toString();
    pendingActions_.insert(requestId, action);
    requestDeadlines_.insert(requestId, QDateTime::currentMSecsSinceEpoch() + 10000);
    socket_.write(protocol::encodeFrame(request));
    return requestId;
}

void ApiClient::readResponses()
{
    QString decodeError;
    const auto messages = decoder_.append(socket_.readAll(), &decodeError);
    if (!decodeError.isEmpty()) {
        emit connectionChanged(false, decodeError);
        socket_.disconnectFromHost();
        return;
    }

    for (const QJsonObject &message : messages) {
        if (message.value(QStringLiteral("type")).toString() == QStringLiteral("event")) {
            emit eventReceived(message.value(QStringLiteral("action")).toString(),
                               message.value(QStringLiteral("data")).toObject());
            continue;
        }
        const QString requestId = message.value(QStringLiteral("requestId")).toString();
        const QString action = pendingActions_.take(requestId);
        requestDeadlines_.remove(requestId);
        const bool ok = message.value(QStringLiteral("ok")).toBool(false);
        const QJsonObject error = message.value(QStringLiteral("error")).toObject();
        emit responseReceived(
            requestId,
            action,
            ok,
            message.value(QStringLiteral("data")).toObject(),
            error.value(QStringLiteral("code")).toString(),
            error.value(QStringLiteral("message")).toString());
    }
}

} // namespace evcs
