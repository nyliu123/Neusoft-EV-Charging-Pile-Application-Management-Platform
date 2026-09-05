#pragma once

#include "data/database_manager.h"

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

    DatabaseManager databaseManager_;
    QTcpServer tcpServer_;
};

} // namespace ev
