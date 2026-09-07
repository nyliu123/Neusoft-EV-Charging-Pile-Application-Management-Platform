#include "server_application.h"

#include "admin_handler.h"
#include "adapters/map_api_adapter.h"
#include "common/error_code.h"
#include "common/protocol.h"
#include "services/admin_auth_service.h"
#include "services/admin_seeder.h"
#include "services/station_service.h"
#include "services/user_service.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QPointer>
#include <QSaveFile>
#include <QTcpSocket>
#include <QUuid>
#include <utility>

namespace ev {

ServerApplication::ServerApplication(QString databasePath, QObject *parent)
    : QObject(parent),
      databaseManager_(databasePath),
      avatarDirectory_(QFileInfo(databasePath).absoluteDir().filePath(QStringLiteral("avatars"))),
      mapApiAdapter_(std::make_unique<MapApiAdapter>(this))
{
    connect(&tcpServer_, &QTcpServer::newConnection,
            this, &ServerApplication::acceptPendingConnections);
}

ServerApplication::~ServerApplication() = default;

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

    adminHandler_ = std::make_unique<AdminHandler>(mainDatabase_, sessionManager_);

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
    if (msgType == MessageType::UserRequest) {
        processUserRequest(socket, frame);
        return;
    }
    if (msgType == MessageType::StationRequest) {
        processStationRequest(socket, frame);
        return;
    }
    if (msgType == MessageType::AdminQuery) {
        processAdminQuery(socket, frame);
        return;
    }
    if (msgType == MessageType::AdminAction) {
        processAdminAction(socket, frame);
        return;
    }

    sendError(socket, requestId, QStringLiteral("message type is not implemented"));
}

void ServerApplication::processUserRequest(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendUserResponse(socket, requestId, false, QStringLiteral("PROTOCOL_ERROR"),
                         QStringLiteral("unsupported protocol version"));
        return;
    }

    const QJsonObject data = frame.payload.value(QStringLiteral("data")).toObject();
    const QString sessionId = data.value(QStringLiteral("session_id")).toString();
    if (!connectionSessions_.value(socket).contains(sessionId)) {
        sendUserResponse(socket, requestId, false, QStringLiteral("UNAUTHORIZED"),
                         QStringLiteral("登录已过期，请重新登录"));
        return;
    }
    const qint64 userId = sessionManager_.authenticatedUserId(sessionId);
    if (userId <= 0) {
        connectionSessions_[socket].remove(sessionId);
        sendUserResponse(socket, requestId, false, QStringLiteral("UNAUTHORIZED"),
                         QStringLiteral("登录已过期，请重新登录"));
        return;
    }

    const QString type = data.value(QStringLiteral("type")).toString();
    const QJsonObject params = data.value(QStringLiteral("params")).toObject();
    const UserService service;

    if (type == QStringLiteral("user_info")) {
        const auto result = service.queryUserInfo(mainDatabase_, userId);
        if (!result.success) {
            if (result.code == ErrorCode::AccountFrozen) {
                sessionManager_.remove(sessionId);
                connectionSessions_[socket].remove(sessionId);
            }
            sendUserResponse(socket, requestId, false, errorCodeName(result.code), result.message);
            return;
        }
        const UserRecord &user = result.data;
        sendUserResponse(socket, requestId, true, QStringLiteral("OK"),
                         QStringLiteral("个人信息已更新"), QJsonObject {
            {QStringLiteral("user_id"), user.userId},
            {QStringLiteral("nickname"), user.nickname},
            {QStringLiteral("avatar_path"), user.avatarPath},
            {QStringLiteral("balance_cent"), user.balanceCent}
        });
        return;
    }

    if (type == QStringLiteral("update_nickname")) {
        const QString nickname = params.value(QStringLiteral("nickname")).toString().trimmed();
        const auto result = service.updateNickname(mainDatabase_, userId, nickname);
        if (result.code == ErrorCode::AccountFrozen) {
            sessionManager_.remove(sessionId);
            connectionSessions_[socket].remove(sessionId);
        }
        sendUserResponse(socket, requestId, result.success, errorCodeName(result.code),
                         result.success ? QStringLiteral("昵称修改成功") : result.message,
                         result.success ? QJsonObject {{QStringLiteral("nickname"), nickname}}
                                        : QJsonObject {});
        return;
    }

    if (type == QStringLiteral("update_avatar")) {
        const QByteArray bytes = QByteArray::fromBase64(
            params.value(QStringLiteral("file_data")).toString().toLatin1());
        const QString mimeType = QMimeDatabase().mimeTypeForData(bytes).name();
        if (bytes.size() < 4 || bytes.size() > 2 * 1024 * 1024
            || mimeType != QStringLiteral("image/jpeg")) {
            sendUserResponse(socket, requestId, false, QStringLiteral("INVALID_INPUT"),
                             QStringLiteral("头像必须是有效的JPG图片，且不超过2MB"));
            return;
        }
        QDir directory(avatarDirectory_);
        if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
            sendUserResponse(socket, requestId, false, QStringLiteral("STORAGE_ERROR"),
                             QStringLiteral("头像保存目录创建失败"));
            return;
        }
        const QString avatarPath = directory.absoluteFilePath(
            QStringLiteral("%1_%2.jpg").arg(userId).arg(QDateTime::currentMSecsSinceEpoch()));
        QSaveFile file(avatarPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
            || !file.commit()) {
            sendUserResponse(socket, requestId, false, QStringLiteral("STORAGE_ERROR"),
                             QStringLiteral("头像上传失败"));
            return;
        }
        const auto result = service.updateAvatarPath(mainDatabase_, userId, avatarPath);
        if (!result.success) {
            QFile::remove(avatarPath);
            if (result.code == ErrorCode::AccountFrozen) {
                sessionManager_.remove(sessionId);
                connectionSessions_[socket].remove(sessionId);
            }
            sendUserResponse(socket, requestId, false, errorCodeName(result.code), result.message);
            return;
        }
        sendUserResponse(socket, requestId, true, QStringLiteral("OK"),
                         QStringLiteral("头像更换成功"),
                         QJsonObject {{QStringLiteral("avatar_path"), avatarPath}});
        return;
    }

    if (type == QStringLiteral("recharge")) {
        const qint64 amountCent = params.value(QStringLiteral("amount_cent")).toInteger();
        const auto result = service.recharge(mainDatabase_, userId, amountCent);
        if (result.code == ErrorCode::AccountFrozen) {
            sessionManager_.remove(sessionId);
            connectionSessions_[socket].remove(sessionId);
        }
        sendUserResponse(socket, requestId, result.success, errorCodeName(result.code),
                         result.success ? QStringLiteral("充值成功") : result.message,
                         result.success
                             ? QJsonObject {{QStringLiteral("balance_cent"), result.data}}
                             : QJsonObject {});
        return;
    }

    sendUserResponse(socket, requestId, false, QStringLiteral("INVALID_INPUT"),
                     QStringLiteral("不支持的用户操作"));
}

void ServerApplication::processStationRequest(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendStationResponse(socket, requestId, false, QStringLiteral("PROTOCOL_ERROR"),
                            QStringLiteral("unsupported protocol version"));
        return;
    }

    const QJsonObject data = frame.payload.value(QStringLiteral("data")).toObject();
    const QString sessionId = data.value(QStringLiteral("session_id")).toString();
    if (!connectionSessions_.value(socket).contains(sessionId)
        || sessionManager_.authenticatedUserId(sessionId) <= 0) {
        connectionSessions_[socket].remove(sessionId);
        sendStationResponse(socket, requestId, false, QStringLiteral("UNAUTHORIZED"),
                            QStringLiteral("登录已过期，请重新登录"));
        return;
    }

    const QString type = data.value(QStringLiteral("type")).toString();
    const QJsonObject params = data.value(QStringLiteral("params")).toObject();
    const StationService service;

    if (type == QStringLiteral("geocode")) {
        const QString address = params.value(QStringLiteral("address")).toString();
        QPointer<QTcpSocket> socketGuard(socket);
        mapApiAdapter_->geocode(address,
            [this, socketGuard, requestId](Result<GeocodeResult> response) {
                if (!socketGuard) {
                    return;
                }
                if (!response.success) {
                    sendStationResponse(socketGuard, requestId, false,
                        errorCodeName(response.code), response.message);
                    return;
                }
                sendStationResponse(socketGuard, requestId, true, QStringLiteral("OK"),
                    QStringLiteral("位置解析成功"), QJsonObject {
                        {QStringLiteral("longitude"), response.data.longitude},
                        {QStringLiteral("latitude"), response.data.latitude},
                        {QStringLiteral("display_address"), response.data.displayAddress},
                        {QStringLiteral("source"), response.data.source},
                        {QStringLiteral("confidence"), response.data.confidence}
                    });
            });
        return;
    }

    if (type == QStringLiteral("station_list")) {
        std::optional<double> longitude;
        std::optional<double> latitude;
        if (params.contains(QStringLiteral("longitude"))
            || params.contains(QStringLiteral("latitude"))) {
            if (!params.contains(QStringLiteral("longitude"))
                || !params.contains(QStringLiteral("latitude"))
                || !params.value(QStringLiteral("longitude")).isDouble()
                || !params.value(QStringLiteral("latitude")).isDouble()) {
                sendStationResponse(socket, requestId, false,
                    QStringLiteral("INVALID_INPUT"), QStringLiteral("经纬度必须同时提供"));
                return;
            }
            longitude = params.value(QStringLiteral("longitude")).toDouble();
            latitude = params.value(QStringLiteral("latitude")).toDouble();
        }
        const auto response = service.listStations(mainDatabase_, longitude, latitude);
        if (!response.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(response.code),
                                response.message);
            return;
        }
        QJsonArray stations;
        for (const StationListItem &item : response.data) {
            QJsonObject station {
                {QStringLiteral("station_id"), item.station.stationId},
                {QStringLiteral("station_name"), item.station.stationName},
                {QStringLiteral("address"), item.station.address},
                {QStringLiteral("price_per_kwh"), item.station.pricePerKwh},
                {QStringLiteral("total_piles"), item.station.totalPiles},
                {QStringLiteral("idle_count"), item.station.idlePiles}
            };
            if (item.station.hasLocation) {
                station.insert(QStringLiteral("longitude"), item.station.longitude);
                station.insert(QStringLiteral("latitude"), item.station.latitude);
            }
            if (item.distanceKm.has_value()) {
                station.insert(QStringLiteral("distance_km"), *item.distanceKm);
            }
            stations.append(station);
        }
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {},
                            QJsonObject {{QStringLiteral("stations"), stations}});
        return;
    }

    if (type == QStringLiteral("station_detail")) {
        const qint64 stationId = params.value(QStringLiteral("station_id")).toInteger();
        const auto response = service.stationDetail(mainDatabase_, stationId);
        if (!response.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(response.code),
                                response.message);
            return;
        }
        const StationDetailRecord &detail = response.data;
        int inUse = 0;
        int reserved = 0;
        int fault = 0;
        QJsonArray piles;
        for (const StationPileRecord &pile : detail.piles) {
            if (pile.status == QStringLiteral("in_use")) {
                ++inUse;
            } else if (pile.status == QStringLiteral("reserved")) {
                ++reserved;
            } else if (pile.status == QStringLiteral("fault")) {
                ++fault;
            }
            piles.append(QJsonObject {
                {QStringLiteral("pile_id"), pile.pileId},
                {QStringLiteral("pile_number"), pile.pileNumber},
                {QStringLiteral("pile_type"), pile.pileType},
                {QStringLiteral("power_kw"), pile.powerKw},
                {QStringLiteral("status"), pile.status}
            });
        }
        const int total = detail.station.totalPiles;
        const double onlineRate = StationService::onlineRate(detail);
        QJsonObject station {
            {QStringLiteral("station_id"), detail.station.stationId},
            {QStringLiteral("station_name"), detail.station.stationName},
            {QStringLiteral("address"), detail.station.address},
            {QStringLiteral("price_per_kwh"), detail.station.pricePerKwh}
        };
        if (detail.station.hasLocation) {
            station.insert(QStringLiteral("longitude"), detail.station.longitude);
            station.insert(QStringLiteral("latitude"), detail.station.latitude);
        }
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {}, QJsonObject {
            {QStringLiteral("station"), station},
            {QStringLiteral("piles"), piles},
            {QStringLiteral("stats"), QJsonObject {
                {QStringLiteral("total"), total},
                {QStringLiteral("idle"), detail.station.idlePiles},
                {QStringLiteral("in_use"), inUse},
                {QStringLiteral("reserved"), reserved},
                {QStringLiteral("fault"), fault},
                {QStringLiteral("online_rate"), onlineRate}
            }}
        });
        return;
    }

    sendStationResponse(socket, requestId, false, QStringLiteral("INVALID_INPUT"),
                        QStringLiteral("不支持的站点操作"));
}

void ServerApplication::sendUserResponse(QTcpSocket *socket, const QString &requestId,
                                         bool success, const QString &code,
                                         const QString &message, const QJsonObject &result)
{
    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), success},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), QJsonObject {{QStringLiteral("result"), result}}}
    };
    socket->write(FrameCodec::encode(static_cast<quint32>(MessageType::UserResponse), response));
}

void ServerApplication::sendStationResponse(QTcpSocket *socket, const QString &requestId,
                                            bool success, const QString &code,
                                            const QString &message,
                                            const QJsonObject &result)
{
    const QJsonObject response {
        {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
        {QStringLiteral("request_id"), requestId},
        {QStringLiteral("success"), success},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), QJsonObject {{QStringLiteral("result"), result}}}
    };
    socket->write(FrameCodec::encode(
        static_cast<quint32>(MessageType::StationResponse), response));
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
            const QString adminSessionId =
                QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!sessionManager_.registerUserSession(adminSessionId,
                                                     authResult.data.adminId)) {
                sendError(socket, requestId, QStringLiteral("cannot create admin session"));
                return;
            }
            connectionSessions_[socket].insert(adminSessionId);
            adminSessions_.insert(adminSessionId);
            responseData = {
                {QStringLiteral("admin_id"), authResult.data.adminId},
                {QStringLiteral("username"), authResult.data.username},
                {QStringLiteral("session_id"), adminSessionId}
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
        adminSessions_.remove(sessionId);
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

void ServerApplication::processAdminQuery(QTcpSocket *socket, const Frame &frame)
{
    processAdminRequest(socket, frame, false);
}

void ServerApplication::processAdminAction(QTcpSocket *socket, const Frame &frame)
{
    processAdminRequest(socket, frame, true);
}

void ServerApplication::processAdminRequest(QTcpSocket *socket, const Frame &frame,
                                            bool isAction)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const QJsonObject requestData = frame.payload.value(QStringLiteral("data")).toObject();
    const QString type = requestData.value(QStringLiteral("type")).toString();
    const QString sessionId = requestData.value(QStringLiteral("session_id")).toString();

    const bool valid = !sessionId.isEmpty()
        && adminSessions_.contains(sessionId)
        && connectionSessions_.value(socket).contains(sessionId)
        && sessionManager_.validateAndTouch(sessionId);

    QJsonObject response;
    if (!valid) {
        response = QJsonObject {
            {QStringLiteral("success"), false},
            {QStringLiteral("code"), QStringLiteral("UNAUTHORIZED")},
            {QStringLiteral("message"), QStringLiteral("登录已过期，请重新登录")},
            {QStringLiteral("data"), QJsonObject {
                {QStringLiteral("type"), type},
                {QStringLiteral("error"), QStringLiteral("session_expired")}
            }}
        };
    } else {
        const QJsonObject params = requestData.value(QStringLiteral("params")).toObject();
        response = isAction
            ? adminHandler_->processAction(type, params)
            : adminHandler_->processQuery(type, params);
    }

    response.insert(QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion));
    response.insert(QStringLiteral("request_id"), requestId);
    socket->write(FrameCodec::encode(static_cast<quint32>(MessageType::AdminResponse),
                                     response));
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
