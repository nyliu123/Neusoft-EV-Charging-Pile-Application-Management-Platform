#pragma once

#include "charging_session_manager.h"
#include "data/database_manager.h"
#include "adapters/map_api_adapter.h"
#include "network/frame_codec.h"
#include "services/session_manager.h"

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QTcpServer>

#include <memory>

namespace ev {

class AdminHandler;
class MapApiAdapter;
class ConsultApiAdapter;

class ServerApplication final : public QObject {
public:
    explicit ServerApplication(QString databasePath, QObject *parent = nullptr);
    // Out-of-line so unique_ptr<AdminHandler> can stay forward-declared here.
    ~ServerApplication() override;
    bool start(const QHostAddress &address, quint16 port);

private:
    void acceptPendingConnections();
    void readClient(QTcpSocket *socket);
    void processFrame(QTcpSocket *socket, const Frame &frame);
    void processLogin(QTcpSocket *socket, const Frame &frame);
    void processSessionHeartbeat(QTcpSocket *socket, const Frame &frame);
    void processLogout(QTcpSocket *socket, const Frame &frame);
    void processUserRequest(QTcpSocket *socket, const Frame &frame);
    void processStationRequest(QTcpSocket *socket, const Frame &frame);
    void processChargeRequest(QTcpSocket *socket, const Frame &frame);
    void processMembershipRequest(QTcpSocket *socket, const Frame &frame);
    void processConsultRequest(QTcpSocket *socket, const Frame &frame);
    Result<qint64> authenticateFeatureUser(QTcpSocket *socket, const Frame &frame);
    void sendFeatureResponse(QTcpSocket *socket, const QString &requestId, quint32 type, const Result<QJsonObject> &result);
    void processAdminQuery(QTcpSocket *socket, const Frame &frame);
    void processAdminAction(QTcpSocket *socket, const Frame &frame);
    void processAdminRequest(QTcpSocket *socket, const Frame &frame, bool isAction);
    void sendError(QTcpSocket *socket, const QString &requestId, const QString &message);
    void sendUserResponse(QTcpSocket *socket, const QString &requestId,
                          bool success, const QString &code, const QString &message,
                          const QJsonObject &result = {});
    void sendStationResponse(QTcpSocket *socket, const QString &requestId,
                             bool success, const QString &code, const QString &message,
                             const QJsonObject &result = {});
    void sendChargeResponse(QTcpSocket *socket, const QString &requestId,
                            bool success, const QString &code, const QString &message,
                            const QJsonObject &result = {});

    DatabaseManager databaseManager_;
    QSqlDatabase mainDatabase_;
    bool databaseReady_ = false;
    QString avatarDirectory_;
    SessionManager sessionManager_;
    ChargingSessionManager chargingSessionManager_;
    QTcpServer tcpServer_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
    QHash<QTcpSocket *, QSet<QString>> connectionSessions_;
    QSet<QString> adminSessions_;
    std::unique_ptr<AdminHandler> adminHandler_;
    std::unique_ptr<MapApiAdapter> mapApiAdapter_;
    std::unique_ptr<ConsultApiAdapter> consultApiAdapter_;
    QTimer renewalTimer_;
    QSet<qint64> consultBusy_;
    QHash<QString, qint64> consultLastAt_;
    QHash<QString, QJsonArray> consultHistory_;
    QHash<QString, quint64> consultEpoch_;
};

} // namespace ev
