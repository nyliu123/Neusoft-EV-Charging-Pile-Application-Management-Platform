#pragma once

#include "admin_session.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace ev {

class PlatformClient;

// Request/response pipeline for AdminQuery (0x60) and AdminAction (0x61).
// sendQuery()/sendAction() attach the current session_id, remember the
// request_id and invoke the callback with (success, result, message) when the
// matching AdminResponse (0x62) arrives. "result" is the "result" object
// inside the response data. When the server reports an expired session,
// sessionExpired() is emitted instead of invoking the callback.
//
// sendGeocode() reuses the user-side StationRequest (0x20) "geocode" type:
// the server accepts any session registered in the SessionManager, so admin
// sessions can resolve addresses through the same endpoint. The response
// envelope of StationResponse (0x21) is identical to AdminResponse.
class AdminApiClient final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(bool success, const QJsonObject &result,
                                        const QString &message)>;

    explicit AdminApiClient(PlatformClient *client, QObject *parent = nullptr);

    void setSession(const AdminSession &session);
    AdminSession session() const { return session_; }
    bool hasSession() const { return session_.isLoggedIn && !session_.sessionId.isEmpty(); }

    // "context" keeps the callback alive: it is skipped when the context
    // object has been destroyed meanwhile. Returns false when the request
    // could not be sent.
    bool sendQuery(const QString &type, const QJsonObject &params,
                   QObject *context, Callback callback);
    // Station filters on multiple pages share the same rarely-changing list.
    bool sendStationOptions(QObject *context, Callback callback);
    bool sendAction(const QString &type, const QJsonObject &params,
                    QObject *context, Callback callback);
    bool sendGeocode(const QString &address, QObject *context, Callback callback);

signals:
    void sessionExpired(const QString &message);

private:
    struct Pending {
        QPointer<QObject> context;
        Callback callback;
    };

    void handleFrame(quint32 messageType, const QJsonObject &payload);
    bool sendRequest(quint32 messageType, const QString &type, const QJsonObject &params,
                     QObject *context, const Callback &callback);

    PlatformClient *client_ = nullptr;
    AdminSession session_;
    QJsonObject stationOptionsCache_;
    QHash<QString, Pending> pending_;
};

} // namespace ev
