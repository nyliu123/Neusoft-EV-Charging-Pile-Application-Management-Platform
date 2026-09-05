#pragma once

#include "data/database_manager.h"
#include "network/frame_codec.h"

#include <QHash>
#include <QHostAddress>
#include <QObject>
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
    void sendError(QTcpSocket *socket, const QString &requestId, const QString &message);

    DatabaseManager databaseManager_;
    QTcpServer tcpServer_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
};

} // namespace ev
