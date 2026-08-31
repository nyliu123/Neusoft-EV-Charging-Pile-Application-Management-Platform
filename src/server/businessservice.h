#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace evcs::server {

class Database;

struct ServiceResult
{
    bool ok = false;
    QJsonObject data;
    QString errorCode;
    QString errorMessage;

    static ServiceResult success(const QJsonObject &data = {});
    static ServiceResult failure(const QString &code, const QString &message);
};

class BusinessService final
{
public:
    explicit BusinessService(Database &database);

    ServiceResult handle(const QString &action,
                         const QJsonObject &payload,
                         const QString &token);

private:
    struct UserContext {
        qint64 id = 0;
        QString username;
        QString role;
    };

    std::optional<UserContext> authenticate(const QString &token,
                                            ServiceResult *failureResult = nullptr) const;
    ServiceResult registerUser(const QJsonObject &payload);
    ServiceResult login(const QJsonObject &payload);
    ServiceResult logout(const QString &token);
    ServiceResult userProfile(const QString &token);
    ServiceResult listStations(const QJsonObject &payload, const QString &token);
    ServiceResult getStation(const QJsonObject &payload, const QString &token);
    ServiceResult createReservation(const QJsonObject &payload, const QString &token);
    ServiceResult cancelReservation(const QJsonObject &payload, const QString &token);
    ServiceResult listReservations(const QString &token);
    ServiceResult startCharging(const QJsonObject &payload, const QString &token);
    ServiceResult chargingStatus(const QJsonObject &payload, const QString &token);
    ServiceResult stopCharging(const QJsonObject &payload, const QString &token);
    ServiceResult listOrders(const QString &token);
    ServiceResult getOrder(const QJsonObject &payload, const QString &token);
    ServiceResult adminDashboard(const QString &token);
    ServiceResult adminAnalytics(const QString &token);
    ServiceResult adminGenerateDemoHistory(const QJsonObject &payload, const QString &token);
    ServiceResult adminListStations(const QString &token);
    ServiceResult adminSaveStation(const QJsonObject &payload, const QString &token);
    ServiceResult adminListChargers(const QJsonObject &payload, const QString &token);
    ServiceResult adminSaveCharger(const QJsonObject &payload, const QString &token);
    ServiceResult adminSetChargerStatus(const QJsonObject &payload, const QString &token);
    ServiceResult adminListUsers(const QString &token);
    ServiceResult adminSetUserStatus(const QJsonObject &payload, const QString &token);
    ServiceResult adminListOrders(const QString &token);
    ServiceResult adminListReservations(const QString &token);
    ServiceResult adminListChargingSessions(const QString &token);
    ServiceResult adminListTariffs(const QString &token);
    ServiceResult adminSaveTariff(const QJsonObject &payload, const QString &token);
    ServiceResult adminListFaults(const QString &token);
    ServiceResult adminSaveFault(const QJsonObject &payload, const QString &token);

    bool expireReservations(QString *errorMessage = nullptr) const;
    std::optional<UserContext> authenticateAdmin(const QString &token,
                                                 ServiceResult *failureResult) const;

    Database &database_;
};

} // namespace evcs::server
