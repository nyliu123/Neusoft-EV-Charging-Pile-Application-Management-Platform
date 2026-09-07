#pragma once

#include "common/result.h"

#include <QJsonObject>
#include <QSqlDatabase>

namespace ev {

class StationService final {
public:
    Result<QJsonObject> queryStations(QSqlDatabase &database,
                                      bool hasLocation = false,
                                      double longitude = 0.0,
                                      double latitude = 0.0) const;
    Result<QJsonObject> queryPiles(QSqlDatabase &database, qint64 stationId) const;
};

} // namespace ev
