#include "data/station_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ev {

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

} // namespace ev
