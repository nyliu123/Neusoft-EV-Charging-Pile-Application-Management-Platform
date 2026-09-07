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
    return sendRequest(QStringLiteral("user_info"), {}, context, std::move(callback));
}

bool UserApiClient::updateNickname(const QString &nickname, QObject *context,
                                   Callback callback)
{
    return sendRequest(QStringLiteral("update_nickname"),
                       {{QStringLiteral("nickname"), nickname}},
                       context, std::move(callback));
}

bool UserApiClient::updateAvatar(const QByteArray &jpegData, QObject *context,
                                 Callback callback)
{
    return sendRequest(QStringLiteral("update_avatar"),
                       {{QStringLiteral("file_data"),
                         QString::fromLatin1(jpegData.toBase64())}},
                       context, std::move(callback));
}

bool UserApiClient::recharge(qint64 amountCent, QObject *context, Callback callback)
{
    return sendRequest(QStringLiteral("recharge"),
                       {{QStringLiteral("amount_cent"), amountCent}},
                       context, std::move(callback));
}

bool UserApiClient::geocode(const QString &address, QObject *context, Callback callback)
{
    return sendRequest(QStringLiteral("geocode"),
                       {{QStringLiteral("address"), address}},
                       context, std::move(callback));
}

bool UserApiClient::queryStations(bool hasLocation, double longitude, double latitude,
                                  QObject *context, Callback callback)
{
    QJsonObject params;
    if (hasLocation) {
        params.insert(QStringLiteral("longitude"), longitude);
        params.insert(QStringLiteral("latitude"), latitude);
    }
    return sendRequest(QStringLiteral("query_stations"), params,
                       context, std::move(callback));
}

bool UserApiClient::queryPiles(qint64 stationId, QObject *context, Callback callback)
{
    return sendRequest(QStringLiteral("query_piles"),
                       {{QStringLiteral("station_id"), stationId}},
                       context, std::move(callback));
}

bool UserApiClient::sendRequest(const QString &type, const QJsonObject &params,
                                QObject *context, Callback callback)
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
        static_cast<quint32>(MessageType::UserRequest), data);
    if (requestId.isEmpty()) {
        callback(false, {}, QStringLiteral("发送请求失败，请检查网络连接"));
        return false;
    }
    pending_.insert(requestId, {QPointer<QObject>(context), std::move(callback)});
    return true;
}

void UserApiClient::handleFrame(quint32 messageType, const QJsonObject &payload)
{
    if (messageType != static_cast<quint32>(MessageType::UserResponse)) {
        return;
    }
    const QString requestId = payload.value(QStringLiteral("request_id")).toString();
    const auto it = pending_.find(requestId);
    if (it == pending_.end()) {
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
