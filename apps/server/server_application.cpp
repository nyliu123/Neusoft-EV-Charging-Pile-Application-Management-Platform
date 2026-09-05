#include "server_application.h"

#include "common/protocol.h"

#include <QDebug>
#include <QJsonObject>
#include <QTcpSocket>
#include <utility>

namespace ev {

ServerApplication::ServerApplication(QString databasePath, QObject *parent)
    : QObject(parent), databaseManager_(std::move(databasePath))
{
    connect(&tcpServer_, &QTcpServer::newConnection,
            this, &ServerApplication::acceptPendingConnections);
}

bool ServerApplication::start(const QHostAddress &address, quint16 port)
{
    auto databaseResult = databaseManager_.openForCurrentThread();
    if (!databaseResult.success) {
        qCritical().noquote() << "database open failed:" << databaseResult.message;
        return false;
    }
    auto migrationResult = databaseManager_.migrate(databaseResult.data);
    if (!migrationResult.success) {
        qCritical().noquote() << "database migration failed:" << migrationResult.message;
        return false;
    }

    if (!tcpServer_.listen(address, port)) {
        qCritical().noquote() << "listen failed:" << tcpServer_.errorString();
        return false;
    }
    qInfo().noquote() << "EV charging server listening on"
                      << tcpServer_.serverAddress().toString()
                      << tcpServer_.serverPort();
    return true;
}

void ServerApplication::acceptPendingConnections()
{
    while (tcpServer_.hasPendingConnections()) {
        QTcpSocket *socket = tcpServer_.nextPendingConnection();
        if (!socket) {
            continue;
        }
        socket->setParent(this);
        receiveBuffers_.insert(socket, {});
        qInfo().noquote() << "client connected:" << socket->peerAddress().toString()
                          << socket->peerPort();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            readClient(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            receiveBuffers_.remove(socket);
            qInfo().noquote() << "client disconnected:" << socket->peerAddress().toString();
            socket->deleteLater();
        });
    }
}

void ServerApplication::readClient(QTcpSocket *socket)
{
    QByteArray &buffer = receiveBuffers_[socket];
    buffer.append(socket->readAll());
    while (!buffer.isEmpty()) {
        const DecodeResult result = FrameCodec::decodeOne(buffer);
        if (result.status == DecodeStatus::NeedMore) {
            return;
        }
        if (result.status == DecodeStatus::Invalid) {
            sendError(socket, {}, result.message);
            socket->disconnectFromHost();
            return;
        }
        processFrame(socket, result.frame);
    }
}

void ServerApplication::processFrame(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    if (frame.messageType != static_cast<quint32>(MessageType::HealthRequest)) {
        sendError(socket, requestId, QStringLiteral("message type is not implemented"));
        return;
    }

    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    const bool compatible = requestedVersion == ProtocolVersion;
    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), compatible},
        {QStringLiteral("code"), compatible ? QStringLiteral("OK")
                                             : QStringLiteral("PROTOCOL_ERROR")},
        {QStringLiteral("message"), compatible ? QStringLiteral("service is ready")
                                                : QStringLiteral("unsupported protocol version")},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("server_version"), QStringLiteral("0.1.0")},
             {QStringLiteral("service"), QStringLiteral("ev_server")}
         }}
    };
    socket->write(FrameCodec::encode(static_cast<quint32>(MessageType::HealthResponse), response));
}

void ServerApplication::sendError(QTcpSocket *socket,
                                  const QString &requestId,
                                  const QString &message)
{
    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), false},
        {QStringLiteral("code"), QStringLiteral("PROTOCOL_ERROR")},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), QJsonObject {}}
    };
    socket->write(FrameCodec::encode(static_cast<quint32>(MessageType::ErrorResponse), response));
}

} // namespace ev
