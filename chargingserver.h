#pragma once

#include "database.h"
#include "mapapiadapter.h"
#include "protocol.h"

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QThreadPool>
#include <QTimer>
#include <QSet>

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
    void configureMap(const QString &apiKey, const QString &referer);
    quint16 serverPort() const;

private:
    void acceptPendingConnections();
    void readClient(QTcpSocket *socket);
    void removeClient(QTcpSocket *socket);
    void sendMessage(QTcpSocket *socket, const QJsonObject &message);
    void dispatchMessage(QTcpSocket *socket, const QJsonObject &message);
    void pushChargingUpdates();
    QJsonObject handleMessage(const QJsonObject &message) const;

    Database database_;
    QString databasePath_;
    QTcpServer server_;
    QThreadPool workerPool_;
    MapApiAdapter mapApi_;
    QHash<QTcpSocket *, protocol::FrameDecoder> decoders_;
    QHash<QTcpSocket *, QString> chargingSubscribers_;
    QSet<QTcpSocket *> chargingPushInFlight_;
    QTimer chargingPushTimer_;
    QHash<QString, QJsonObject> responseCache_;
    QSet<QString> inFlightRequests_;
};

} // namespace evcs::server
