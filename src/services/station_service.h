#pragma once

#include "common/result.h"
#include "data/station_repository.h"

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
