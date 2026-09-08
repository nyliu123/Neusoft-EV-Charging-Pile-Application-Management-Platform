#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace ev {

struct StationRecord {
    qint64 stationId = 0;
    QString stationName;
    QString address;
    double longitude = 0.0;
    double latitude = 0.0;
    bool hasLocation = false;
    double pricePerKwh = 0.0;
    int totalPiles = 0;
    int idlePiles = 0;
};

struct StationPileRecord {
    qint64 pileId = 0;
    QString pileNumber;
    QString pileType;
    double powerKw = 0.0;
    QString status;
};

struct StationDetailRecord {
    StationRecord station;
    QVector<StationPileRecord> piles;
};

class StationRepository final {
public:
    Result<QVector<StationRecord>> listStations(QSqlDatabase &database) const;
    Result<std::optional<StationDetailRecord>> findDetail(
        QSqlDatabase &database, qint64 stationId) const;
    // UML-018: nearby stations sorted by Haversine distance.
    Result<QVector<StationRecord>> findNearby(QSqlDatabase &database,
                                               double longitude,
                                               double latitude) const;
    // UML-042: insert a new station. Returns new station_id.
    Result<qint64> insertStation(QSqlDatabase &database,
                                  const QString &name,
                                  const QString &address,
                                  double longitude,
                                  double latitude,
                                  double pricePerKwh) const;
};

} // namespace ev
