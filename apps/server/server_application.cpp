#include "server_application.h"

#include "admin_handler.h"
#include "adapters/map_api_adapter.h"
#include "adapters/consult_api_adapter.h"
#include "services/membership_service.h"
#include "common/error_code.h"
#include "common/protocol.h"
#include "services/admin_auth_service.h"
#include "services/admin_seeder.h"
#include "services/charge_service.h"
#include "services/comment_service.h"
#include "services/order_service.h"
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

namespace {

QJsonObject chargePileJson(const PileRecord &pile)
{
    return QJsonObject {
        {QStringLiteral("pile_id"), pile.pileId},
        {QStringLiteral("pile_number"), pile.pileNumber},
        {QStringLiteral("pile_type"), pile.pileType},
        {QStringLiteral("power_kw"), pile.powerKw},
        {QStringLiteral("status"), pile.status}
    };
}

QJsonObject chargeOrderJson(const OrderRecord &order)
{
    return QJsonObject {
        {QStringLiteral("order_id"), order.orderId},
        {QStringLiteral("status"), order.status},
        {QStringLiteral("pile_id"), order.pileId},
        {QStringLiteral("station_id"), order.stationId},
        {QStringLiteral("reserve_time"), order.reserveTime},
        {QStringLiteral("start_time"), order.startTime},
        {QStringLiteral("charge_amount_kwh"), order.chargeAmountKwh},
        {QStringLiteral("price_per_kwh"), order.pricePerKwh},
        {QStringLiteral("total_fee_cent"), order.totalFeeCent},
        {"gross_fee_cent",order.grossFeeCent},
        {"discount_fee_cent",order.grossFeeCent-order.totalFeeCent},
        {"membership_level",order.membershipLevel},{"discount_bps",order.discountBps}
    };
}

} // namespace

ServerApplication::ServerApplication(QString databasePath, QObject *parent)
    : QObject(parent),
      databaseManager_(databasePath),
      avatarDirectory_(QFileInfo(databasePath).absoluteDir().filePath(QStringLiteral("avatars"))),
      mapApiAdapter_(std::make_unique<MapApiAdapter>(this))
{
    consultApiAdapter_ = std::make_unique<ConsultApiAdapter>(this);
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
    chargingSessionManager_.setDatabase(mainDatabase_);

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
    renewalTimer_.setInterval(30000);
    connect(&renewalTimer_, &QTimer::timeout, this, [this] {
        const auto result = MembershipService().processDue(mainDatabase_);
        if (!result.success) qWarning() << "membership renewal processing failed";
    });
    if (!tcpServer_.listen(address, port)) {
        qCritical().noquote() << "listen failed:" << tcpServer_.errorString();
        return false;
    }
    renewalTimer_.start();
    const auto renewals = MembershipService().processDue(mainDatabase_);
    if (!renewals.success) qWarning() << "membership startup renewal processing failed";
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
            chargingSessionManager_.detachSocket(socket);
            connectionSessions_.remove(socket);
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
    if (msgType == MessageType::ChargeRequest) {
        processChargeRequest(socket, frame);
        return;
    }
    if (msgType == MessageType::MembershipRequest) { processMembershipRequest(socket, frame); return; }
    if (msgType == MessageType::ConsultRequest) { processConsultRequest(socket, frame); return; }
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

    if (type == QStringLiteral("query_orders")) {
        if (adminSessions_.contains(sessionId)) {
            sendUserResponse(socket, requestId, false, QStringLiteral("UNAUTHORIZED"),
                             QStringLiteral("请使用用户账号查询个人订单"));
            return;
        }
        // User identity is resolved above; client-supplied user_id is never trusted.
        const auto result = OrderService().queryOrders(mainDatabase_, userId);
        if (result.code == ErrorCode::AccountFrozen || result.code == ErrorCode::Unauthorized) {
            sessionManager_.remove(sessionId);
            connectionSessions_[socket].remove(sessionId);
        }
        sendUserResponse(socket, requestId, result.success, errorCodeName(result.code),
                         result.message, result.data);
        return;
    }

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
    const qint64 userId = sessionManager_.authenticatedUserId(sessionId);
    if (!connectionSessions_.value(socket).contains(sessionId)
        || adminSessions_.contains(sessionId)
        || userId <= 0) {
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
        const CommentService commentService;
        const auto summaries = commentService.loadSummaries(mainDatabase_);
        if (!summaries.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(summaries.code),
                                summaries.message);
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
            station.insert(QStringLiteral("rating"),
                CommentService::summaryJson(
                    summaries.data.value(item.station.stationId)));
            stations.append(station);
        }
        const auto membership = MembershipService().snapshot(mainDatabase_, userId);
        const QJsonObject pricing = membership.success ? membership.data : QJsonObject{};
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {},
                            QJsonObject {{QStringLiteral("stations"), stations},
                                {"membership_level", pricing.value("level").toString("NORMAL")},
                                {"discount_bps", pricing.value("discount_bps").toInt(10000)}});
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
        const CommentService commentService;
        const auto rating = commentService.stationSummary(
            mainDatabase_, detail.station.stationId);
        if (!rating.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(rating.code),
                                rating.message);
            return;
        }
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
        station.insert(QStringLiteral("rating"),
            rating.data.has_value()
                ? CommentService::summaryJson(rating.data.value())
                : CommentService::summaryJson(StationRatingSummary{}));
        const auto membership = MembershipService().snapshot(mainDatabase_, userId);
        const QJsonObject pricing = membership.success ? membership.data : QJsonObject{};
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {}, QJsonObject {
            {QStringLiteral("station"), station},
            {QStringLiteral("piles"), piles},
            {"membership_level", pricing.value("level").toString("NORMAL")},
            {"discount_bps", pricing.value("discount_bps").toInt(10000)},
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

    if (type == QStringLiteral("list_comments")) {
        const qint64 stationId = params.value(QStringLiteral("station_id")).toInteger();
        const qint64 viewerId = sessionManager_.authenticatedUserId(sessionId);
        const CommentService commentService;
        const auto response = commentService.listComments(mainDatabase_, stationId,
                                                          viewerId);
        if (!response.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(response.code),
                                response.message);
            return;
        }
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {},
                            response.data);
        return;
    }

    if (type == QStringLiteral("post_comment")) {
        const qint64 stationId = params.value(QStringLiteral("station_id")).toInteger();
        const QString content = params.value(QStringLiteral("content")).toString();
        const int rating = params.value(QStringLiteral("rating")).toInt();
        const qint64 userId = sessionManager_.authenticatedUserId(sessionId);
        const CommentService commentService;
        const auto response = commentService.postComment(
            mainDatabase_, userId, stationId, content, rating);
        if (!response.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(response.code),
                                response.message);
            return;
        }
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"),
                            response.data.value(QStringLiteral("updated")).toBool()
                                ? QStringLiteral("评论已更新")
                                : QStringLiteral("评论已发表"),
                            response.data);
        return;
    }

    if (type == QStringLiteral("toggle_like")) {
        const qint64 commentId = params.value(QStringLiteral("comment_id")).toInteger();
        const qint64 userId = sessionManager_.authenticatedUserId(sessionId);
        const CommentService commentService;
        const auto response = commentService.toggleLike(mainDatabase_, userId,
                                                        commentId);
        if (!response.success) {
            sendStationResponse(socket, requestId, false, errorCodeName(response.code),
                                response.message);
            return;
        }
        sendStationResponse(socket, requestId, true, QStringLiteral("OK"), {},
                            response.data);
        return;
    }

    sendStationResponse(socket, requestId, false, QStringLiteral("INVALID_INPUT"),
                        QStringLiteral("不支持的站点操作"));
}

void ServerApplication::processChargeRequest(QTcpSocket *socket, const Frame &frame)
{
    const QString requestId = frame.payload.value(QStringLiteral("request_id")).toString();
    const auto requestedVersion = static_cast<quint32>(
        frame.payload.value(QStringLiteral("protocol_version")).toInteger());
    if (requestedVersion != ProtocolVersion) {
        sendChargeResponse(socket, requestId, false, QStringLiteral("PROTOCOL_ERROR"),
                           QStringLiteral("unsupported protocol version"));
        return;
    }

    const QJsonObject data = frame.payload.value(QStringLiteral("data")).toObject();
    const QString sessionId = data.value(QStringLiteral("session_id")).toString();
    if (!connectionSessions_.value(socket).contains(sessionId)
        || sessionManager_.authenticatedUserId(sessionId) <= 0) {
        connectionSessions_[socket].remove(sessionId);
        sendChargeResponse(socket, requestId, false, QStringLiteral("UNAUTHORIZED"),
                           QStringLiteral("登录已过期，请重新登录"));
        return;
    }
    const qint64 userId = sessionManager_.authenticatedUserId(sessionId);

    const QString type = data.value(QStringLiteral("type")).toString();
    const QJsonObject params = data.value(QStringLiteral("params")).toObject();
    const ChargeService service;

    if (type == QStringLiteral("check_pending")) {
        const auto result = service.checkPending(mainDatabase_, userId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        QJsonObject order;
        if (result.data.has_value()) {
            const PendingOrderInfo &info = *result.data;
            order = chargeOrderJson(info.order);
            order.insert(QStringLiteral("pile_number"), info.pileNumber);
            order.insert(QStringLiteral("pile_type"), info.pileType);
            order.insert(QStringLiteral("power_kw"), info.powerKw);
            order.insert(QStringLiteral("station_name"), info.stationName);
            if (info.liveSnapshot.has_value()) {
                order.insert(QStringLiteral("live"), QJsonObject {
                    {QStringLiteral("elapsed_sec"), info.liveSnapshot->elapsedSec},
                    {QStringLiteral("charge_amount_kwh"), info.liveSnapshot->kwh},
                    {QStringLiteral("current_fee_cent"), info.liveSnapshot->feeCent},
                    {"gross_fee_cent",info.liveSnapshot->grossFeeCent},
                    {QStringLiteral("progress"), info.liveSnapshot->progressPercent}
                });
                // Re-bind the socket so 0x32 pushes resume after a reconnect.
                chargingSessionManager_.ensureSession(info.order, info.powerKw, socket);
            }
        }
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"), {},
                           QJsonObject {
                               {QStringLiteral("has_pending"), result.data.has_value()},
                               {QStringLiteral("order"), order}
                           });
        return;
    }

    if (type == QStringLiteral("check_pile")) {
        const qint64 pileId = params.value(QStringLiteral("pile_id")).toInteger();
        const auto result = service.checkPile(mainDatabase_, pileId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        const PileCheckInfo &info = result.data;
        const auto membership = MembershipService().snapshot(mainDatabase_, userId);
        const QJsonObject pricing = membership.success ? membership.data : QJsonObject{};
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"), {}, QJsonObject {
            {QStringLiteral("available"), info.available},
            {QStringLiteral("reason"), info.reason},
            {QStringLiteral("pile"), chargePileJson(info.pile)},
            {QStringLiteral("station_name"), info.stationName},
            {QStringLiteral("price_per_kwh"), info.pricePerKwh},
            {"membership_level", pricing.value("level").toString("NORMAL")},
            {"discount_bps", pricing.value("discount_bps").toInt(10000)}
        });
        return;
    }

    if (type == QStringLiteral("reserve")) {
        const qint64 pileId = params.value(QStringLiteral("pile_id")).toInteger();
        const auto result = service.reserve(mainDatabase_, userId, pileId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        const ReserveOutcome &outcome = result.data;
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"),
                           QStringLiteral("预约成功"), QJsonObject {
                               {QStringLiteral("order_id"), outcome.orderId},
                               {QStringLiteral("pile"), chargePileJson(outcome.pile)},
                               {QStringLiteral("station_name"), outcome.stationName},
                               {QStringLiteral("price_per_kwh"), outcome.pricePerKwh},
                               {"membership_level",outcome.membershipLevel},{"discount_bps",outcome.discountBps}
                           });
        return;
    }

    if (type == QStringLiteral("start_charge")) {
        const qint64 orderId = params.value(QStringLiteral("order_id")).toInteger();
        const auto result = service.startCharge(mainDatabase_, userId, orderId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        const StartedCharge &charge = result.data;
        chargingSessionManager_.ensureSession(charge.order, charge.powerKw, socket);
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"),
                           QStringLiteral("充电已开始"), QJsonObject {
                               {QStringLiteral("order_id"), charge.order.orderId},
                               {QStringLiteral("start_time"), charge.order.startTime},
                               {QStringLiteral("power_kw"), charge.powerKw},
                               {QStringLiteral("price_per_kwh"), charge.order.pricePerKwh},
                               {"membership_level",charge.order.membershipLevel},{"discount_bps",charge.order.discountBps}
                           });
        return;
    }

    if (type == QStringLiteral("end_charge")) {
        const qint64 orderId = params.value(QStringLiteral("order_id")).toInteger();
        const auto result = service.endCharge(mainDatabase_, userId, orderId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        chargingSessionManager_.stopSession(orderId);
        const SettleOutcome &outcome = result.data;
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"),
                           outcome.settled ? QStringLiteral("结算完成")
                                           : QStringLiteral("余额不足，订单待结算"),
                           QJsonObject {
                               {QStringLiteral("settled"), outcome.settled},
                               {QStringLiteral("order_id"), outcome.orderId},
                               {QStringLiteral("total_kwh"), outcome.totalKwh},
                               {QStringLiteral("total_fee_cent"), outcome.totalFeeCent},
                               {"gross_fee_cent",outcome.grossFeeCent},
                               {"discount_fee_cent",outcome.grossFeeCent-outcome.totalFeeCent},
                               {"membership_level",outcome.membershipLevel},{"discount_bps",outcome.discountBps},
                               {QStringLiteral("balance_cent"), outcome.balanceCent},
                               {QStringLiteral("shortfall_cent"), outcome.shortfallCent}
                           });
        return;
    }

    if (type == QStringLiteral("cancel_charge")) {
        const qint64 orderId = params.value(QStringLiteral("order_id")).toInteger();
        const auto result = service.cancel(mainDatabase_, userId, orderId);
        if (!result.success) {
            sendChargeResponse(socket, requestId, false, errorCodeName(result.code),
                               result.message);
            return;
        }
        sendChargeResponse(socket, requestId, true, QStringLiteral("OK"),
                           QStringLiteral("预约已取消"),
                           QJsonObject {{QStringLiteral("order_id"), orderId}});
        return;
    }

    sendChargeResponse(socket, requestId, false, QStringLiteral("INVALID_INPUT"),
                       QStringLiteral("不支持的充电操作"));
}

void ServerApplication::sendChargeResponse(QTcpSocket *socket, const QString &requestId,
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
        static_cast<quint32>(MessageType::ChargeResponse), response));
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
    const bool valid = sessionManager_.validateAndTouch(sessionId);
    if (!valid) {
        connectionSessions_[socket].remove(sessionId);
        adminSessions_.remove(sessionId);
        consultHistory_.remove(sessionId);
        consultLastAt_.remove(sessionId);
        consultEpoch_.remove(sessionId);
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

    for (auto sessions = connectionSessions_.begin();
         sessions != connectionSessions_.end(); ++sessions) {
        if (sessions.key() != socket) {
            sessions.value().remove(sessionId);
        }
    }
    connectionSessions_[socket].insert(sessionId);

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
        consultHistory_.remove(sessionId);
        consultEpoch_.remove(sessionId);
        consultLastAt_.remove(sessionId);
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

    const qint64 authenticatedAdminId = sessionManager_.authenticatedUserId(sessionId);
    const bool valid = !sessionId.isEmpty()
        && adminSessions_.contains(sessionId)
        && connectionSessions_.value(socket).contains(sessionId)
        && authenticatedAdminId > 0;

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
            ? adminHandler_->processAction(type, params, authenticatedAdminId)
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
