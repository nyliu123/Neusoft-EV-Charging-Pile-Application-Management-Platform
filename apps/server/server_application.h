#pragma once

#include "data/database_manager.h"
#include "network/frame_codec.h"
#include "services/session_manager.h"

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QTcpServer>

#include <memory>

namespace ev {

class AdminHandler;

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
    void processAdminQuery(QTcpSocket *socket, const Frame &frame);
    void processAdminAction(QTcpSocket *socket, const Frame &frame);
    void processAdminRequest(QTcpSocket *socket, const Frame &frame, bool isAction);
    void sendError(QTcpSocket *socket, const QString &requestId, const QString &message);

    DatabaseManager databaseManager_;
    QSqlDatabase mainDatabase_;
    bool databaseReady_ = false;
    SessionManager sessionManager_;
    QTcpServer tcpServer_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
    QHash<QTcpSocket *, QSet<QString>> connectionSessions_;
    QSet<QString> adminSessions_;
    std::unique_ptr<AdminHandler> adminHandler_;
};

} // namespace ev
