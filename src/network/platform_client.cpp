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

    connect(&socket_, &QTcpSocket::connected, this, [this] {
        reconnectAttempt_ = 0;
        setState(State::Connected, QStringLiteral("TCP 已连接，正在检查服务"));
        sendHealthCheck();
    });
    connect(&socket_, &QTcpSocket::readyRead, this, &PlatformClient::readFrames);
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        setState(State::Disconnected, QStringLiteral("与服务端断开"));
        scheduleReconnect();
    });
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        setState(State::Disconnected, socket_.errorString());
        scheduleReconnect();
    });
    connect(&reconnectTimer_, &QTimer::timeout, this, &PlatformClient::reconnectNow);
}

PlatformClient::~PlatformClient()
{
    reconnectTimer_.stop();
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
            setState(State::Connected,
                     result.frame.payload.value(QStringLiteral("message")).toString());
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
