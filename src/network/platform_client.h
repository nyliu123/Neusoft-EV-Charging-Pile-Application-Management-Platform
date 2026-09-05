#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

namespace ev {

class PlatformClient final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Ready
    };
    Q_ENUM(State)

    explicit PlatformClient(QString clientName, QObject *parent = nullptr);
    ~PlatformClient() override;

    void connectToServer(QString host, quint16 port);
    void reconnectNow();
    void login(const QString &phone);
    void logout();
    State state() const;

signals:
    void stateChanged(ev::PlatformClient::State state, const QString &detail);
    void healthCheckSucceeded(const QString &serverVersion);
    void loginSucceeded(const QJsonObject &userInfo, bool isNewUser);
    void loginFailed(const QString &code, const QString &message);
    void sessionExpired(const QString &message);
    void logoutFinished(bool success, const QString &message);

private:
    void setState(State state, const QString &detail);
    void sendHealthCheck();
    void sendLoginRequest(const QString &phone, bool isAutoRegister);
    void activateUserSession(const QString &sessionId);
    void sendSessionHeartbeat();
    void clearUserSession();
    void failPendingLogin(const QString &code, const QString &message);
    void readFrames();
    void scheduleReconnect();

    QString clientName_;
    QString host_;
    quint16 port_ = 0;
    State state_ = State::Disconnected;
    int reconnectAttempt_ = 0;
    int maxReconnectAttempts_ = 10;
    QByteArray receiveBuffer_;
    QTcpSocket socket_;
    QTimer reconnectTimer_;
    QTimer loginTimer_;
    QTimer sessionHeartbeatTimer_;
    QTimer sessionResponseTimer_;
    QString pendingLoginRequestId_;
    QString pendingLoginPhone_;
    bool pendingAutoRegistration_ = false;
    QString sessionId_;
    QString heartbeatRequestId_;
    QString logoutRequestId_;
};

} // namespace ev
