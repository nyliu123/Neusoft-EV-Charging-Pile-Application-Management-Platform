#include "user_api_client.h"

#include "common/protocol.h"
#include "network/platform_client.h"
#include "user_session_state.h"

#include <utility>

namespace ev {

UserApiClient::UserApiClient(PlatformClient *client, QObject *parent)
    : QObject(parent), client_(client)
{
    connect(client_, &PlatformClient::frameReceived,
            this, &UserApiClient::handleFrame);
    connect(client_, &PlatformClient::sessionExpired, this,
            [this](const QString &message) {
        pending_.clear();
        emit sessionExpired(message);
    });
}

bool UserApiClient::queryUserInfo(QObject *context, Callback callback)
{
    return sendUserRequest(QStringLiteral("user_info"), {}, context, std::move(callback));
}

bool UserApiClient::updateNickname(const QString &nickname, QObject *context,
                                   Callback callback)
{
    return sendUserRequest(QStringLiteral("update_nickname"),
                       {{QStringLiteral("nickname"), nickname}},
                       context, std::move(callback));
}

bool UserApiClient::updateAvatar(const QByteArray &jpegData, QObject *context,
                                 Callback callback)
{
    return sendUserRequest(QStringLiteral("update_avatar"),
                       {{QStringLiteral("file_data"),
                         QString::fromLatin1(jpegData.toBase64())}},
                       context, std::move(callback));
}

bool UserApiClient::recharge(qint64 amountCent, QObject *context, Callback callback)
{
    return sendUserRequest(QStringLiteral("recharge"),
                       {{QStringLiteral("amount_cent"), amountCent}},
                       context, std::move(callback));
}

bool UserApiClient::geocode(const QString &address, QObject *context, Callback callback)
{
    return sendStationRequest(QStringLiteral("geocode"),
                              {{QStringLiteral("address"), address}},
                              context, std::move(callback));
}

bool UserApiClient::queryStations(bool hasLocation, double longitude, double latitude,
                                  QObject *context, Callback callback)
{
    if (hasLocation) {
        return queryStations(longitude, latitude, context, std::move(callback));
    }
    return queryStations(context, std::move(callback));
}

bool UserApiClient::queryPiles(qint64 stationId, QObject *context, Callback callback)
{
    return queryStationDetail(stationId, context, std::move(callback));
}

bool UserApiClient::queryStations(QObject *context, Callback callback)
{
    return sendStationRequest(QStringLiteral("station_list"), {}, context,
                              std::move(callback));
}

bool UserApiClient::queryStations(double longitude, double latitude,
                                  QObject *context, Callback callback)
{
    return sendStationRequest(QStringLiteral("station_list"), {
        {QStringLiteral("longitude"), longitude},
        {QStringLiteral("latitude"), latitude}
    }, context, std::move(callback));
}

bool UserApiClient::queryStationDetail(qint64 stationId, QObject *context,
                                       Callback callback)
{
    return sendStationRequest(QStringLiteral("station_detail"),
                              {{QStringLiteral("station_id"), stationId}},
                              context, std::move(callback));
}

bool UserApiClient::checkPendingCharge(QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("check_pending"), {},
                             context, std::move(callback));
}

bool UserApiClient::checkPile(qint64 pileId, QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("check_pile"),
                             {{QStringLiteral("pile_id"), pileId}},
                             context, std::move(callback));
}

bool UserApiClient::reserveCharge(qint64 pileId, QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("reserve"),
                             {{QStringLiteral("pile_id"), pileId}},
                             context, std::move(callback));
}

bool UserApiClient::startCharge(qint64 orderId, QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("start_charge"),
                             {{QStringLiteral("order_id"), orderId}},
                             context, std::move(callback));
}

bool UserApiClient::endCharge(qint64 orderId, QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("end_charge"),
                             {{QStringLiteral("order_id"), orderId}},
                             context, std::move(callback));
}

bool UserApiClient::cancelCharge(qint64 orderId, QObject *context, Callback callback)
{
    return sendChargeRequest(QStringLiteral("cancel_charge"),
                             {{QStringLiteral("order_id"), orderId}},
                             context, std::move(callback));
}

bool UserApiClient::sendUserRequest(const QString &type, const QJsonObject &params,
                                    QObject *context, Callback callback)
{
    return sendRequest(type, params, context, std::move(callback),
                       static_cast<quint32>(MessageType::UserRequest),
                       static_cast<quint32>(MessageType::UserResponse));
}

bool UserApiClient::sendStationRequest(const QString &type, const QJsonObject &params,
                                       QObject *context, Callback callback)
{
    return sendRequest(type, params, context, std::move(callback),
                       static_cast<quint32>(MessageType::StationRequest),
                       static_cast<quint32>(MessageType::StationResponse));
}

bool UserApiClient::sendChargeRequest(const QString &type, const QJsonObject &params,
                                      QObject *context, Callback callback)
{
    return sendRequest(type, params, context, std::move(callback),
                       static_cast<quint32>(MessageType::ChargeRequest),
                       static_cast<quint32>(MessageType::ChargeResponse));
}

bool UserApiClient::sendRequest(const QString &type, const QJsonObject &params,
                                QObject *context, Callback callback,
                                quint32 requestType, quint32 responseType)
{
    const UserSessionState &session = UserSessionState::instance();
    if (!session.isLoggedIn()) {
        emit sessionExpired(QStringLiteral("登录已过期，请重新登录"));
        return false;
    }
    const QJsonObject data {
        {QStringLiteral("type"), type},
        {QStringLiteral("params"), params},
        {QStringLiteral("session_id"), session.sessionId()}
    };
    const QString requestId = client_->sendFrame(
        requestType, data);
    if (requestId.isEmpty()) {
        callback(false, {}, QStringLiteral("发送请求失败，请检查网络连接"));
        return false;
    }
    pending_.insert(requestId,
                    {QPointer<QObject>(context), std::move(callback), responseType});
    return true;
}

void UserApiClient::handleFrame(quint32 messageType, const QJsonObject &payload)
{
    // Server push (0x32) carries no request_id and must bypass the pending map.
    if (messageType == static_cast<quint32>(MessageType::ChargeUpdate)) {
        emit chargeUpdateReceived(payload);
        return;
    }
    const QString requestId = payload.value(QStringLiteral("request_id")).toString();
    const auto it = pending_.find(requestId);
    if (it == pending_.end()) {
        return;
    }
    if (messageType != it.value().responseType) {
        return;
    }
    const Pending pending = it.value();
    pending_.erase(it);

    const bool success = payload.value(QStringLiteral("success")).toBool();
    const QString code = payload.value(QStringLiteral("code")).toString();
    const QString message = payload.value(QStringLiteral("message")).toString();
    if (!success && (code == QStringLiteral("UNAUTHORIZED")
                     || code == QStringLiteral("ACCOUNT_FROZEN"))) {
        client_->invalidateSession(message);
        return;
    }
    if (pending.context.isNull()) {
        return;
    }
    const QJsonObject result = payload.value(QStringLiteral("data")).toObject()
                                   .value(QStringLiteral("result")).toObject();
    pending.callback(success, result, message);
}

} // namespace ev
