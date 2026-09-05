#include "server_application.h"

#include "common/protocol.h"
#include "services/admin_auth_service.h"
#include "services/admin_seeder.h"

#include <QDebug>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>
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
    mainDatabase_ = databaseResult.data;
    databaseReady_ = true;

    auto migrationResult = databaseManager_.migrate(mainDatabase_);
    if (!migrationResult.success) {
        qCritical().noquote() << "database migration failed:" << migrationResult.message;
        return false;
    }

    // Seed default admin if the admins table is empty.
    auto seedResult = AdminSeeder::seedIfNeeded(mainDatabase_);
    if (!seedResult.success) {
        qCritical().noquote() << "admin seeding failed:" << seedResult.message;
        return false;
    }
    if (seedResult.data > 0) {
        qInfo().noquote() << "seeded default admin account:"
                          << AdminSeeder::kDefaultUsername
                          << "/" << AdminSeeder::kDefaultPassword;
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
    const auto msgType = static_cast<MessageType>(frame.messageType);

    if (msgType == MessageType::HealthRequest) {
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
        return;
    }

    if (msgType == MessageType::LoginRequest) {
        const QJsonObject data = frame.payload.value(QStringLiteral("data")).toObject();
        const QString username = data.value(QStringLiteral("username")).toString();
        const QString password = data.value(QStringLiteral("password")).toString();
        const QString role = data.value(QStringLiteral("role")).toString();

        if (role != QStringLiteral("admin")) {
            // User login is handled by the user client branch.
            sendError(socket, requestId, QStringLiteral("role not implemented yet"));
            return;
        }

        if (!databaseReady_) {
            sendError(socket, requestId, QStringLiteral("database is not ready"));
            return;
        }

        const Result<AdminInfo> authResult = AdminAuthService::authenticate(
            username, password, mainDatabase_);

        QJsonObject responseData;
        if (authResult.success) {
            responseData = {
                {QStringLiteral("admin_id"), authResult.data.adminId},
                {QStringLiteral("username"), authResult.data.username}
            };
            qInfo().noquote() << "admin login succeeded:" << authResult.data.username
                              << "from" << socket->peerAddress().toString();
        } else {
            responseData = {};
            qInfo().noquote() << "admin login failed for" << username
                              << "from" << socket->peerAddress().toString()
                              << "-" << authResult.message;
        }

        const QJsonObject response {
            {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
            {QStringLiteral("request_id"), requestId},
            {QStringLiteral("success"), authResult.success},
            {QStringLiteral("code"), errorCodeName(authResult.code)},
            {QStringLiteral("message"), authResult.message},
            {QStringLiteral("data"), responseData}
        };
        socket->write(FrameCodec::encode(static_cast<quint32>(MessageType::LoginResponse), response));
        return;
    }

    sendError(socket, requestId, QStringLiteral("message type is not implemented"));
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
