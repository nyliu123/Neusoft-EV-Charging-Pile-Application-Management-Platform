#pragma once

#include "common/result.h"
#include "data/station_repository.h"

#include <QJsonObject>
#include <QSqlDatabase>
#include <QVector>
#include <optional>

namespace ev {

struct StationListItem {
    StationRecord station;
    std::optional<double> distanceKm;
};

class StationService final {
public:
    // JSON compatibility API retained for the existing client and tests.
    Result<QJsonObject> queryStations(QSqlDatabase &database,
                                      bool hasLocation = false,
                                      double longitude = 0.0,
                                      double latitude = 0.0) const;
    Result<QJsonObject> queryPiles(QSqlDatabase &database, qint64 stationId) const;

    Result<QVector<StationListItem>> listStations(
        QSqlDatabase &database,
        std::optional<double> longitude = std::nullopt,
        std::optional<double> latitude = std::nullopt) const;
    Result<StationDetailRecord> stationDetail(
        QSqlDatabase &database, qint64 stationId) const;

    static bool isValidCoordinate(double longitude, double latitude);
    static double haversineKm(double firstLongitude, double firstLatitude,
                              double secondLongitude, double secondLatitude);
    static double onlineRate(const StationDetailRecord &detail);

private:
    StationRepository repository_;
};

} // namespace ev
