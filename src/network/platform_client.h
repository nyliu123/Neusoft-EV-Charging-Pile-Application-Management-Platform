#pragma once

#include "network/frame_codec.h"

#include <QByteArray>
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
    State state() const;

    // Send a frame to the server. Returns the request_id used.
    QString sendFrame(quint32 messageType, const QJsonObject &data);

signals:
    void stateChanged(ev::PlatformClient::State state, const QString &detail);
    void healthCheckSucceeded(const QString &serverVersion);

    // Emitted when a complete frame is received (excluding health/error handled internally).
    void frameReceived(quint32 messageType, const QJsonObject &payload);

private:
    void setState(State state, const QString &detail);
    void sendHealthCheck();
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
};

} // namespace ev
