#pragma once

#include "common/error_code.h"

#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

namespace ev {

class SessionManager;

// Dispatches AdminQuery (0x60) / AdminAction (0x61) requests of the admin
// client. Each entry point returns the response payload without
// protocol_version / request_id; ServerApplication injects them before
// sending the AdminResponse (0x62) frame.
//
// Response "data" object shape:
//   { "type": <echoed request type>, "result": <query or action payload> }
class AdminHandler final {
public:
    AdminHandler(QSqlDatabase database, SessionManager &sessionManager);

    QJsonObject processQuery(const QString &type, const QJsonObject &params);
    QJsonObject processAction(const QString &type, const QJsonObject &params);

private:
    // Queries (UML-035 ~ 038 / 040 / 041 / 043 / 046).
    QJsonObject queryDashboardOverview(const QJsonObject &params);
    QJsonObject queryDashboardSummary();
    QJsonObject queryRevenueTrend(const QJsonObject &params);
    QJsonObject queryPileStatusStats();
    QJsonObject queryPileList(const QJsonObject &params);
    QJsonObject queryStationList();
    QJsonObject queryStationDetail(const QJsonObject &params);
    QJsonObject queryUserList(const QJsonObject &params);
    QJsonObject queryOrderList(const QJsonObject &params);

    // Actions (UML-039 / 042 / 045).
    QJsonObject actionRestartPile(const QJsonObject &params);
    QJsonObject actionAddStation(const QJsonObject &params);
    QJsonObject actionSetUserStatus(const QJsonObject &params);

    static QJsonObject okBody(const QString &type, const QJsonObject &result,
                              const QString &message = QString());
    static QJsonObject failBody(const QString &type, ErrorCode code,
                                const QString &message);

    QSqlDatabase database_;
    SessionManager &sessionManager_;
};

} // namespace ev
