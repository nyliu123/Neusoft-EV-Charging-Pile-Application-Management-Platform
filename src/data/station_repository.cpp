#include "data/station_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace ev {

namespace {

// Haversine formula — great-circle distance in kilometres.
double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kEarthRadiusKm = 6371.0;
    constexpr double kDeg2Rad = 3.141592653589793 / 180.0;
    const double dLat = (lat2 - lat1) * kDeg2Rad;
    const double dLon = (lon2 - lon1) * kDeg2Rad;
    const double aLat1 = lat1 * kDeg2Rad;
    const double aLat2 = lat2 * kDeg2Rad;
    const double sinDLat = std::sin(dLat * 0.5);
    const double sinDLon = std::sin(dLon * 0.5);
    const double a = sinDLat * sinDLat
        + std::cos(aLat1) * std::cos(aLat2) * sinDLon * sinDLon;
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthRadiusKm * c;
}

} // namespace

namespace {

StationRecord stationFromQuery(const QSqlQuery &query)
{
    StationRecord station;
    station.stationId = query.value(0).toLongLong();
    station.stationName = query.value(1).toString();
    station.address = query.value(2).toString();
    station.hasLocation = !query.value(3).isNull() && !query.value(4).isNull();
    station.longitude = query.value(3).toDouble();
    station.latitude = query.value(4).toDouble();
    station.pricePerKwh = query.value(5).toDouble();
    station.totalPiles = query.value(6).toInt();
    station.idlePiles = query.value(7).toInt();
    return station;
}

const QString stationSelectSql()
{
    return QStringLiteral(
        "SELECT s.station_id, s.station_name, s.address, s.longitude, s.latitude, "
        "s.price_per_kwh, COUNT(p.pile_id), "
        "COALESCE(SUM(CASE WHEN p.status = 'idle' THEN 1 ELSE 0 END), 0) "
        "FROM charging_stations s "
        "LEFT JOIN charging_piles p ON p.station_id = s.station_id ");
}

} // namespace

Result<QVector<StationRecord>> StationRepository::listStations(
    QSqlDatabase &database) const
{
    QSqlQuery query(database);
    if (!query.exec(stationSelectSql()
                    + QStringLiteral("GROUP BY s.station_id ORDER BY s.station_id"))) {
        return Result<QVector<StationRecord>>::fail(ErrorCode::StorageError,
                                                     query.lastError().text());
    }

    QVector<StationRecord> stations;
    while (query.next()) {
        stations.append(stationFromQuery(query));
    }
    return Result<QVector<StationRecord>>::ok(stations);
}

Result<std::optional<StationDetailRecord>> StationRepository::findDetail(
    QSqlDatabase &database, qint64 stationId) const
{
    QSqlQuery stationQuery(database);
    stationQuery.prepare(stationSelectSql()
        + QStringLiteral("WHERE s.station_id = ? GROUP BY s.station_id LIMIT 1"));
    stationQuery.addBindValue(stationId);
    if (!stationQuery.exec()) {
        return Result<std::optional<StationDetailRecord>>::fail(
            ErrorCode::StorageError, stationQuery.lastError().text());
    }
    if (!stationQuery.next()) {
        return Result<std::optional<StationDetailRecord>>::ok(std::nullopt);
    }

    StationDetailRecord detail;
    detail.station = stationFromQuery(stationQuery);

    QSqlQuery pileQuery(database);
    pileQuery.prepare(QStringLiteral(
        "SELECT pile_id, pile_number, pile_type, power_kw, status "
        "FROM charging_piles WHERE station_id = ? ORDER BY pile_number, pile_id"));
    pileQuery.addBindValue(stationId);
    if (!pileQuery.exec()) {
        return Result<std::optional<StationDetailRecord>>::fail(
            ErrorCode::StorageError, pileQuery.lastError().text());
    }
    while (pileQuery.next()) {
        StationPileRecord pile;
        pile.pileId = pileQuery.value(0).toLongLong();
        pile.pileNumber = pileQuery.value(1).toString();
        pile.pileType = pileQuery.value(2).toString();
        pile.powerKw = pileQuery.value(3).toDouble();
        pile.status = pileQuery.value(4).toString();
        detail.piles.append(pile);
    }
    return Result<std::optional<StationDetailRecord>>::ok(detail);
}

// ── UML-018: nearby stations ───────────────────────────────────────────────

Result<QVector<StationRecord>> StationRepository::findNearby(
    QSqlDatabase &database, double longitude, double latitude) const
{
    QSqlQuery query(database);
    if (!query.exec(stationSelectSql()
                    + QStringLiteral("GROUP BY s.station_id ORDER BY s.station_id"))) {
        return Result<QVector<StationRecord>>::fail(ErrorCode::StorageError,
                                                      query.lastError().text());
    }
    QVector<StationRecord> stations;
    while (query.next()) {
        StationRecord s = stationFromQuery(query);
        // Calculate distance if station has location.
        if (s.hasLocation) {
            // Store distance in pricePerKw temporarily — not ideal but avoids
            // changing the struct. The upper layer can use it for sorting display.
            // In production we'd add a 'distanceKm' field to StationRecord.
        }
        stations.append(s);
    }
    // Sort by Haversine distance.
    std::sort(stations.begin(), stations.end(),
              [&](const StationRecord &a, const StationRecord &b) {
                  if (!a.hasLocation) return false;
                  if (!b.hasLocation) return true;
                  return haversineKm(latitude, longitude, a.latitude, a.longitude)
                       < haversineKm(latitude, longitude, b.latitude, b.longitude);
              });
    return Result<QVector<StationRecord>>::ok(stations);
}

// ── UML-042: insert new station ────────────────────────────────────────────

Result<qint64> StationRepository::insertStation(
    QSqlDatabase &database,
    const QString &name,
    const QString &address,
    double longitude,
    double latitude,
    double pricePerKwh) const
{
    if (pricePerKwh <= 0.0) {
        return Result<qint64>::fail(ErrorCode::InvalidInput,
                                    QStringLiteral("price_must_be_positive"));
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_stations "
        "(station_name, address, longitude, latitude, price_per_kwh) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(address);
    query.addBindValue(longitude);
    query.addBindValue(latitude);
    query.addBindValue(pricePerKwh);
    if (!query.exec()) {
        return Result<qint64>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<qint64>::ok(query.lastInsertId().toLongLong());
}

} // namespace ev
