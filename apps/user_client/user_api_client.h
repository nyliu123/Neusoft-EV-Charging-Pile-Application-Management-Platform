#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <functional>

namespace ev {

class PlatformClient;

class UserApiClient final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(bool success, const QJsonObject &result,
                                        const QString &message)>;

    explicit UserApiClient(PlatformClient *client, QObject *parent = nullptr);

    bool queryUserInfo(QObject *context, Callback callback);
    bool queryOrders(QObject *context, Callback callback);
    bool membership(const QString &type, const QJsonObject &params, QObject *context, Callback callback);
    bool consult(const QString &type, const QJsonObject &params, QObject *context, Callback callback);
    bool updateNickname(const QString &nickname, QObject *context, Callback callback);
    bool updateAvatar(const QByteArray &jpegData, QObject *context, Callback callback);
    bool recharge(qint64 amountCent, QObject *context, Callback callback);
    bool geocode(const QString &address, QObject *context, Callback callback);
    // Compatibility overloads used by the existing station-search page.
    bool queryStations(bool hasLocation, double longitude, double latitude,
                       QObject *context, Callback callback);
    bool queryPiles(qint64 stationId, QObject *context, Callback callback);
    bool queryStations(QObject *context, Callback callback);
    bool queryStations(double longitude, double latitude,
                       QObject *context, Callback callback);
    bool queryStationDetail(qint64 stationId, QObject *context, Callback callback);
    // UML-025~032 charging flow (ChargeRequest=0x30 / ChargeResponse=0x31).
    bool checkPendingCharge(QObject *context, Callback callback);
    bool checkPile(qint64 pileId, QObject *context, Callback callback);
    bool reserveCharge(qint64 pileId, QObject *context, Callback callback);
    bool startCharge(qint64 orderId, QObject *context, Callback callback);
    bool endCharge(qint64 orderId, QObject *context, Callback callback);
    bool cancelCharge(qint64 orderId, QObject *context, Callback callback);
    // Station comments (StationRequest=0x20: list_comments / post_comment /
    // toggle_like). Station list and detail responses carry a rating summary.
    bool listComments(qint64 stationId, QObject *context, Callback callback);
    bool postComment(qint64 stationId, const QString &content, int rating,
                     QObject *context, Callback callback);
    bool toggleCommentLike(qint64 commentId, QObject *context, Callback callback);

signals:
    void sessionExpired(const QString &message);
    // Server-pushed live charge data (ChargeUpdate=0x32, no request_id).
    void chargeUpdateReceived(const QJsonObject &update);

private:
    struct Pending {
        QPointer<QObject> context;
        Callback callback;
        quint32 responseType = 0;
    };

    bool sendRequest(const QString &type, const QJsonObject &params,
                     QObject *context, Callback callback,
                     quint32 requestType, quint32 responseType);
    bool sendUserRequest(const QString &type, const QJsonObject &params,
                         QObject *context, Callback callback);
    bool sendStationRequest(const QString &type, const QJsonObject &params,
                            QObject *context, Callback callback);
    bool sendChargeRequest(const QString &type, const QJsonObject &params,
                           QObject *context, Callback callback);
    void handleFrame(quint32 messageType, const QJsonObject &payload);

    PlatformClient *client_ = nullptr;
    QHash<QString, Pending> pending_;
};

} // namespace ev
