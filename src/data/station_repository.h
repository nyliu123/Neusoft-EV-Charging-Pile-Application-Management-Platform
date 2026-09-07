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
};

} // namespace ev
