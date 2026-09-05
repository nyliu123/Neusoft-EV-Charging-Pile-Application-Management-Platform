#include "network/platform_client.h"

#include "common/protocol.h"
#include "network/frame_codec.h"

#include <QJsonObject>
#include <QUuid>
#include <utility>

namespace ev {

PlatformClient::PlatformClient(QString clientName, QObject *parent)
    : QObject(parent), clientName_(std::move(clientName))
{
    reconnectTimer_.setSingleShot(true);
    reconnectTimer_.setInterval(1000);
    loginTimer_.setSingleShot(true);
    loginTimer_.setInterval(10000);
    sessionHeartbeatTimer_.setInterval(30000);
    sessionResponseTimer_.setSingleShot(true);
    sessionResponseTimer_.setInterval(3000);

    connect(&socket_, &QTcpSocket::connected, this, [this] {
        reconnectAttempt_ = 0;
        setState(State::Connected, QStringLiteral("TCP 已连接，正在检查服务"));
        sendHealthCheck();
    });
    connect(&socket_, &QTcpSocket::readyRead, this, &PlatformClient::readFrames);
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        failPendingLogin(QStringLiteral("CONNECTION_LOST"),
                         QStringLiteral("网络连接已断开，请稍后重试"));
        const bool hadSession = !sessionId_.isEmpty();
        clearUserSession();
        if (hadSession) {
            emit sessionExpired(QStringLiteral("登录已过期，请重新登录"));
        }
        setState(State::Disconnected, QStringLiteral("与服务端断开"));
        scheduleReconnect();
    });
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        setState(State::Disconnected, socket_.errorString());
        scheduleReconnect();
    });
    connect(&reconnectTimer_, &QTimer::timeout, this, &PlatformClient::reconnectNow);
    connect(&loginTimer_, &QTimer::timeout, this, [this] {
        failPendingLogin(QStringLiteral("TIMEOUT"),
                         QStringLiteral("登录请求超时，请稍后重试"));
    });
    connect(&sessionHeartbeatTimer_, &QTimer::timeout,
            this, &PlatformClient::sendSessionHeartbeat);
    connect(&sessionResponseTimer_, &QTimer::timeout, this, [this] {
        if (!heartbeatRequestId_.isEmpty()) {
            clearUserSession();
            emit sessionExpired(QStringLiteral("登录已过期，请重新登录"));
            return;
        }
        if (!logoutRequestId_.isEmpty()) {
            logoutRequestId_.clear();
            sessionHeartbeatTimer_.start();
            emit logoutFinished(false, QStringLiteral("退出登录请求超时，请稍后重试"));
        }
    });
}

PlatformClient::~PlatformClient()
{
    reconnectTimer_.stop();
    loginTimer_.stop();
    clearUserSession();
    socket_.disconnect(this);
    socket_.abort();
}

void PlatformClient::connectToServer(QString host, quint16 port)
{
    host_ = std::move(host);
    port_ = port;
    reconnectAttempt_ = 0;
    reconnectNow();
}

void PlatformClient::reconnectNow()
{
    if (host_.isEmpty() || port_ == 0
        || socket_.state() == QAbstractSocket::ConnectedState
        || socket_.state() == QAbstractSocket::ConnectingState) {
        return;
    }
    reconnectTimer_.stop();
    receiveBuffer_.clear();
    setState(State::Connecting,
             QStringLiteral("正在连接 %1:%2").arg(host_).arg(port_));
    socket_.connectToHost(host_, port_);
}

PlatformClient::State PlatformClient::state() const
{
    return state_;
}

QString PlatformClient::sendFrame(quint32 messageType, const QJsonObject &data)
{
    if (socket_.state() != QAbstractSocket::ConnectedState
        && socket_.state() != QAbstractSocket::ConnectingState) {
        return {};
    }
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("data"), data}
    };
    socket_.write(FrameCodec::encode(messageType, payload));
    return requestId;
}

void PlatformClient::login(const QString &phone)
{
    if (state_ != State::Ready) {
        emit loginFailed(QStringLiteral("NOT_CONNECTED"),
                         QStringLiteral("服务端尚未连接，请稍后重试"));
        return;
    }
    sendLoginRequest(phone, false);
}

void PlatformClient::logout()
{
    if (sessionId_.isEmpty()) {
        emit logoutFinished(true, QStringLiteral("退出登录成功"));
        return;
    }
    if (state_ != State::Ready) {
        clearUserSession();
        emit logoutFinished(true, QStringLiteral("退出登录成功"));
        return;
    }
    if (!logoutRequestId_.isEmpty()) {
        return;
    }

    sessionHeartbeatTimer_.stop();
    sessionResponseTimer_.stop();
    heartbeatRequestId_.clear();
    logoutRequestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), logoutRequestId_},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("session_id"), sessionId_}
         }}
    };
    socket_.write(FrameCodec::encode(static_cast<quint32>(MessageType::LogoutRequest), payload));
    sessionResponseTimer_.start();
}

void PlatformClient::setState(State state, const QString &detail)
{
    state_ = state;
    emit stateChanged(state, detail);
}

void PlatformClient::sendHealthCheck()
{
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("client"), clientName_}
         }}
    };
    socket_.write(FrameCodec::encode(static_cast<quint32>(MessageType::HealthRequest), payload));
}

void PlatformClient::sendLoginRequest(const QString &phone, bool isAutoRegister)
{
    if (!pendingLoginRequestId_.isEmpty()) {
        return;
    }
    pendingLoginRequestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pendingLoginPhone_ = phone;
    pendingAutoRegistration_ = isAutoRegister;
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), pendingLoginRequestId_},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("phone"), phone},
             {QStringLiteral("is_auto_register"), isAutoRegister}
         }}
    };
    socket_.write(FrameCodec::encode(static_cast<quint32>(MessageType::LoginRequest), payload));
    loginTimer_.start();
}

void PlatformClient::failPendingLogin(const QString &code, const QString &message)
{
    if (pendingLoginRequestId_.isEmpty()) {
        return;
    }
    loginTimer_.stop();
    pendingLoginRequestId_.clear();
    pendingLoginPhone_.clear();
    pendingAutoRegistration_ = false;
    emit loginFailed(code, message);
}

void PlatformClient::activateUserSession(const QString &sessionId)
{
    clearUserSession();
    sessionId_ = sessionId;
    if (sessionId_.isEmpty()) {
        return;
    }
    sessionHeartbeatTimer_.start();
    sendSessionHeartbeat();
}

void PlatformClient::sendSessionHeartbeat()
{
    if (state_ != State::Ready || sessionId_.isEmpty()
        || !heartbeatRequestId_.isEmpty() || !logoutRequestId_.isEmpty()) {
        return;
    }
    heartbeatRequestId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject payload {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), heartbeatRequestId_},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("session_id"), sessionId_}
         }}
    };
    socket_.write(FrameCodec::encode(
        static_cast<quint32>(MessageType::SessionHeartbeatRequest), payload));
    sessionResponseTimer_.start();
}

void PlatformClient::clearUserSession()
{
    sessionHeartbeatTimer_.stop();
    sessionResponseTimer_.stop();
    sessionId_.clear();
    heartbeatRequestId_.clear();
    logoutRequestId_.clear();
}

void PlatformClient::readFrames()
{
    receiveBuffer_.append(socket_.readAll());
    while (!receiveBuffer_.isEmpty()) {
        const DecodeResult result = FrameCodec::decodeOne(receiveBuffer_);
        if (result.status == DecodeStatus::NeedMore) {
            return;
        }
        if (result.status == DecodeStatus::Invalid) {
            socket_.disconnectFromHost();
            setState(State::Disconnected, QStringLiteral("服务端返回了无效协议帧"));
            return;
        }

        if (result.frame.messageType == static_cast<quint32>(MessageType::ErrorResponse)) {
            const QString code = result.frame.payload.value(QStringLiteral("code")).toString();
            if (code == QStringLiteral("PROTOCOL_ERROR")) {
                setState(State::Connected,
                         result.frame.payload.value(QStringLiteral("message")).toString());
            }
            continue;
        }
        if (result.frame.messageType == static_cast<quint32>(MessageType::HealthResponse)) {
            if (!result.frame.payload.value(QStringLiteral("success")).toBool()) {
                setState(State::Connected,
                         result.frame.payload.value(QStringLiteral("message")).toString());
                continue;
            }
            const QJsonObject data = result.frame.payload.value(QStringLiteral("data")).toObject();
            const QString version = data.value(QStringLiteral("server_version")).toString();
            setState(State::Ready, QStringLiteral("服务可用，协议握手成功"));
            emit healthCheckSucceeded(version);
            continue;
        }
        if (result.frame.messageType == static_cast<quint32>(MessageType::LoginResponse)) {
            const QString requestId =
                result.frame.payload.value(QStringLiteral("request_id")).toString();
            if (pendingLoginRequestId_.isEmpty() || requestId != pendingLoginRequestId_) {
                // Not our pending login — pass through as generic frame.
                emit frameReceived(result.frame.messageType, result.frame.payload);
                continue;
            }

            loginTimer_.stop();
            const QString phone = pendingLoginPhone_;
            const bool wasAutoRegistration = pendingAutoRegistration_;
            pendingLoginRequestId_.clear();
            pendingLoginPhone_.clear();
            pendingAutoRegistration_ = false;
            if (result.frame.payload.value(QStringLiteral("success")).toBool()) {
                const QJsonObject data =
                    result.frame.payload.value(QStringLiteral("data")).toObject();
                const QJsonObject userInfo =
                    data.value(QStringLiteral("user_info")).toObject();
                activateUserSession(userInfo.value(QStringLiteral("session_id")).toString());
                emit loginSucceeded(userInfo,
                                    data.value(QStringLiteral("is_new_user")).toBool());
                continue;
            }

            const QString error =
                result.frame.payload.value(QStringLiteral("error")).toString();
            if (error == QStringLiteral("user_not_found") && !wasAutoRegistration) {
                sendLoginRequest(phone, true);
                continue;
            }
            emit loginFailed(result.frame.payload.value(QStringLiteral("code")).toString(),
                             result.frame.payload.value(QStringLiteral("message")).toString());
            continue;
        }
        if (result.frame.messageType
            == static_cast<quint32>(MessageType::SessionHeartbeatResponse)) {
            const QString requestId =
                result.frame.payload.value(QStringLiteral("request_id")).toString();
            if (heartbeatRequestId_.isEmpty() || requestId != heartbeatRequestId_) {
                continue;
            }
            sessionResponseTimer_.stop();
            heartbeatRequestId_.clear();
            if (!result.frame.payload.value(QStringLiteral("success")).toBool()) {
                const QString message =
                    result.frame.payload.value(QStringLiteral("message")).toString();
                clearUserSession();
                emit sessionExpired(message.isEmpty()
                    ? QStringLiteral("登录已过期，请重新登录") : message);
            }
            continue;
        }
        if (result.frame.messageType == static_cast<quint32>(MessageType::LogoutResponse)) {
            const QString requestId =
                result.frame.payload.value(QStringLiteral("request_id")).toString();
            if (logoutRequestId_.isEmpty() || requestId != logoutRequestId_) {
                continue;
            }
            sessionResponseTimer_.stop();
            const bool success =
                result.frame.payload.value(QStringLiteral("success")).toBool();
            const QString message =
                result.frame.payload.value(QStringLiteral("message")).toString();
            if (success) {
                clearUserSession();
            } else {
                logoutRequestId_.clear();
                sessionHeartbeatTimer_.start();
            }
            emit logoutFinished(success, message);
            continue;
        }

        // All other frames are forwarded via the generic signal.
        emit frameReceived(result.frame.messageType, result.frame.payload);
    }
}

void PlatformClient::scheduleReconnect()
{
    if (reconnectTimer_.isActive()) {
        return;
    }
    if (reconnectAttempt_ >= maxReconnectAttempts_) {
        setState(State::Disconnected,
                 QStringLiteral("连续 10 次连接失败，请检查服务端地址和端口"));
        return;
    }
    ++reconnectAttempt_;
    reconnectTimer_.start();
}

} // namespace ev
