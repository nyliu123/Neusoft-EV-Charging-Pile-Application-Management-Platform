#include "server_application.h"

#include "common/error_code.h"
#include "common/protocol.h"
#include "services/admin_auth_service.h"
#include "services/admin_seeder.h"
#include "services/user_service.h"

#include <QDebug>
#include <QDateTime>
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
        connectionSessions_.insert(socket, {});
        qInfo().noquote() << "client connected:" << socket->peerAddress().toString()
                          << socket->peerPort();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            readClient(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            sessionManager_.removeAll(connectionSessions_.take(socket));
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
        processLogin(socket, frame);
        return;
    }
    if (msgType == MessageType::SessionHeartbeatRequest) {
        processSessionHeartbeat(socket, frame);
        return;
    }
    if (msgType == MessageType::LogoutRequest) {
        processLogout(socket, frame);
        return;
    }

    sendError(socket, requestId, QStringLiteral("message type is not implemented"));
}

void ServerApplication::processLogin(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendError(socket, requestId, QStringLiteral("unsupported protocol version"));
        return;
    }

    const QJsonObject requestData = frame.payload.value(QStringLiteral("data")).toObject();
    const QString role = requestData.value(QStringLiteral("role")).toString();

    // Admin login path.
    if (role == QStringLiteral("admin")) {
        if (!databaseReady_) {
            sendError(socket, requestId, QStringLiteral("database is not ready"));
            return;
        }

        const QString username = requestData.value(QStringLiteral("username")).toString();
        const QString password = requestData.value(QStringLiteral("password")).toString();

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
        socket->write(FrameCodec::encode(
            static_cast<quint32>(MessageType::LoginResponse), response));
        return;
    }

    // User login path (default).
    const QString phone = requestData.value(QStringLiteral("phone")).toString();
    const bool isAutoRegister =
        requestData.value(QStringLiteral("is_auto_register")).toBool();

    if (!databaseReady_) {
        sendError(socket, requestId, QStringLiteral("database is not ready"));
        return;
    }

    const UserService service;
    const Result<LoginUserInfo> loginResult = isAutoRegister
        ? service.registerAutomatically(mainDatabase_, phone)
        : service.loginExistingUser(mainDatabase_, phone);
    if (!loginResult.success) {
        QString message = loginResult.message;
        QString error = loginResult.message;
        if (loginResult.code == ErrorCode::StorageError) {
            qWarning().noquote() << "login query failed:" << loginResult.message;
            message = isAutoRegister
                ? QStringLiteral("注册失败，请稍后重试")
                : QStringLiteral("登录失败，请稍后重试");
            error = message;
        }
        const QJsonObject response {
            {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
            {QStringLiteral("request_id"), requestId},
            {QStringLiteral("success"), false},
            {QStringLiteral("code"), errorCodeName(loginResult.code)},
            {QStringLiteral("message"), message},
            {QStringLiteral("error"), error},
            {QStringLiteral("data"), QJsonObject {}}
        };
        socket->write(FrameCodec::encode(
            static_cast<quint32>(MessageType::LoginResponse), response));
        return;
    }

    const LoginUserInfo &user = loginResult.data;
    if (!sessionManager_.registerUserSession(user.sessionId, user.userId)) {
        sendError(socket, requestId, QStringLiteral("cannot create user session"));
        return;
    }
    connectionSessions_[socket].insert(user.sessionId);
    const QJsonObject userInfo {
        {QStringLiteral("user_id"), user.userId},
        {QStringLiteral("nickname"), user.nickname},
        {QStringLiteral("avatar_path"), user.avatarPath},
        {QStringLiteral("balance"), static_cast<double>(user.balanceCent) / 100.0},
        {QStringLiteral("session_id"), user.sessionId}
    };
    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), true},
        {QStringLiteral("code"), QStringLiteral("OK")},
        {QStringLiteral("message"), user.isNewUser
            ? QStringLiteral("注册成功，欢迎加入！")
            : QStringLiteral("登录成功")},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("user_info"), userInfo},
             {QStringLiteral("is_new_user"), user.isNewUser}
         }}
    };
    socket->write(FrameCodec::encode(
        static_cast<quint32>(MessageType::LoginResponse), response));
}

void ServerApplication::processSessionHeartbeat(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendError(socket, requestId, QStringLiteral("unsupported protocol version"));
        return;
    }
    const QJsonObject requestData = frame.payload.value(QStringLiteral("data")).toObject();
    const QString sessionId = requestData.value(QStringLiteral("session_id")).toString();
    const bool validForConnection = connectionSessions_.value(socket).contains(sessionId);
    const bool valid = validForConnection && sessionManager_.validateAndTouch(sessionId);
    if (!valid) {
        connectionSessions_[socket].remove(sessionId);
        const QJsonObject response {
            {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
            {QStringLiteral("request_id"), requestId},
            {QStringLiteral("success"), false},
            {QStringLiteral("code"), QStringLiteral("UNAUTHORIZED")},
            {QStringLiteral("message"), QStringLiteral("登录已过期，请重新登录")},
            {QStringLiteral("error"), QStringLiteral("session_expired")},
            {QStringLiteral("data"), QJsonObject {}}
        };
        socket->write(FrameCodec::encode(
            static_cast<quint32>(MessageType::SessionHeartbeatResponse), response));
        return;
    }

    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), true},
        {QStringLiteral("code"), QStringLiteral("OK")},
        {QStringLiteral("message"), QStringLiteral("session is active")},
        {QStringLiteral("data"), QJsonObject {
             {QStringLiteral("server_time"),
              QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
         }}
    };
    socket->write(FrameCodec::encode(
        static_cast<quint32>(MessageType::SessionHeartbeatResponse), response));
}

void ServerApplication::processLogout(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendError(socket, requestId, QStringLiteral("unsupported protocol version"));
        return;
    }
    const QJsonObject requestData = frame.payload.value(QStringLiteral("data")).toObject();
    const QString sessionId = requestData.value(QStringLiteral("session_id")).toString();
    if (connectionSessions_.value(socket).contains(sessionId)) {
        sessionManager_.remove(sessionId);
        connectionSessions_[socket].remove(sessionId);
    }

    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), true},
        {QStringLiteral("code"), QStringLiteral("OK")},
        {QStringLiteral("message"), QStringLiteral("退出登录成功")},
        {QStringLiteral("data"), QJsonObject {}}
    };
    socket->write(FrameCodec::encode(
        static_cast<quint32>(MessageType::LogoutResponse), response));
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
