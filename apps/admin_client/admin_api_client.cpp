#include "admin_api_client.h"

#include "common/protocol.h"
#include "network/platform_client.h"

namespace ev {

AdminApiClient::AdminApiClient(PlatformClient *client, QObject *parent)
    : QObject(parent), client_(client)
{
    connect(client_, &PlatformClient::frameReceived,
            this, &AdminApiClient::handleFrame);
}

void AdminApiClient::setSession(const AdminSession &session)
{
    session_ = session;
}

bool AdminApiClient::sendQuery(const QString &type, const QJsonObject &params,
                               QObject *context, Callback callback)
{
    return sendRequest(static_cast<quint32>(MessageType::AdminQuery),
                       type, params, context, std::move(callback));
}

bool AdminApiClient::sendAction(const QString &type, const QJsonObject &params,
                                QObject *context, Callback callback)
{
    return sendRequest(static_cast<quint32>(MessageType::AdminAction),
                       type, params, context, std::move(callback));
}

bool AdminApiClient::sendRequest(quint32 messageType, const QString &type,
                                 const QJsonObject &params, QObject *context,
                                 const Callback &callback)
{
    if (!hasSession()) {
        emit sessionExpired(QStringLiteral("登录已过期，请重新登录"));
        return false;
    }

    const QJsonObject data {
        {QStringLiteral("type"), type},
        {QStringLiteral("params"), params},
        {QStringLiteral("session_id"), session_.sessionId}
    };
    const QString requestId = client_->sendFrame(messageType, data);
    if (requestId.isEmpty()) {
        callback(false, QJsonObject {},
                 QStringLiteral("发送请求失败，请检查网络连接"));
        return false;
    }

    pending_.insert(requestId, {QPointer<QObject>(context), callback});
    return true;
}

void AdminApiClient::handleFrame(quint32 messageType, const QJsonObject &payload)
{
    if (messageType != static_cast<quint32>(MessageType::AdminResponse)) {
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
    const QJsonObject data = payload.value(QStringLiteral("data")).toObject();
    const QJsonObject result = data.value(QStringLiteral("result")).toObject();

    if (!success && code == QStringLiteral("UNAUTHORIZED")) {
        setSession(AdminSession {});
        emit sessionExpired(message.isEmpty()
            ? QStringLiteral("登录已过期，请重新登录") : message);
        return;
    }

    if (pending.context.isNull()) {
        return;
    }
    pending.callback(success, result, message);
}

} // namespace ev
