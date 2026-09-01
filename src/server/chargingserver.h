#pragma once

#include "database.h"
#include "protocol.h"

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QThreadPool>

class QTcpSocket;

namespace evcs::server {

class ChargingServer final : public QObject
{
    Q_OBJECT

public:
    explicit ChargingServer(QObject *parent = nullptr);
    ~ChargingServer() override;

    bool initialize(const QString &databasePath,
                    const QString &schemaPath,
                    QString *errorMessage = nullptr);
    bool listen(const QHostAddress &address, quint16 port, QString *errorMessage = nullptr);
    quint16 serverPort() const;

private:
    void acceptPendingConnections();
    void readClient(QTcpSocket *socket);
    void removeClient(QTcpSocket *socket);
    void sendMessage(QTcpSocket *socket, const QJsonObject &message);
    void dispatchMessage(QTcpSocket *socket, const QJsonObject &message);
    QJsonObject handleMessage(const QJsonObject &message) const;

    Database database_;
    QString databasePath_;
    QTcpServer server_;
    QThreadPool workerPool_;
    QHash<QTcpSocket *, protocol::FrameDecoder> decoders_;
};

} // namespace evcs::server
