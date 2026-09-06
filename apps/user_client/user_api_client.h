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
    bool updateNickname(const QString &nickname, QObject *context, Callback callback);
    bool updateAvatar(const QByteArray &jpegData, QObject *context, Callback callback);
    bool recharge(qint64 amountCent, QObject *context, Callback callback);

signals:
    void sessionExpired(const QString &message);

private:
    struct Pending {
        QPointer<QObject> context;
        Callback callback;
    };

    bool sendRequest(const QString &type, const QJsonObject &params,
                     QObject *context, Callback callback);
    void handleFrame(quint32 messageType, const QJsonObject &payload);

    PlatformClient *client_ = nullptr;
    QHash<QString, Pending> pending_;
};

} // namespace ev
