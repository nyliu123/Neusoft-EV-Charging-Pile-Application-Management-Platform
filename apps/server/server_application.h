#pragma once

#include "data/database_manager.h"
#include "network/frame_codec.h"
#include "services/session_manager.h"

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSet>
#include <QTcpServer>

namespace ev {

class ServerApplication final : public QObject {
public:
    ServerApplication(QString databasePath, QObject *parent = nullptr);
    bool start(const QHostAddress &address, quint16 port);

private:
    void acceptPendingConnections();
    void readClient(QTcpSocket *socket);
    void processFrame(QTcpSocket *socket, const Frame &frame);
    void processLogin(QTcpSocket *socket, const Frame &frame);
    void processSessionHeartbeat(QTcpSocket *socket, const Frame &frame);
    void processLogout(QTcpSocket *socket, const Frame &frame);
    void sendError(QTcpSocket *socket, const QString &requestId, const QString &message);

    DatabaseManager databaseManager_;
    SessionManager sessionManager_;
    QTcpServer tcpServer_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
    QHash<QTcpSocket *, QSet<QString>> connectionSessions_;
};

} // namespace ev
