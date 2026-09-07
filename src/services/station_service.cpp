#include "services/station_service.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace {

double radians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

double haversine(double latitude1, double longitude1,
                 double latitude2, double longitude2)
{
    const double latitudeDelta = radians(latitude2 - latitude1);
    const double longitudeDelta = radians(longitude2 - longitude1);
    const double a = std::pow(std::sin(latitudeDelta / 2.0), 2.0)
        + std::cos(radians(latitude1)) * std::cos(radians(latitude2))
            * std::pow(std::sin(longitudeDelta / 2.0), 2.0);
    return 6371.0 * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

} // namespace

namespace ev {

Result<QJsonObject> StationService::queryStations(QSqlDatabase &database,
                                                   bool hasLocation,
                                                   double longitude,
                                                   double latitude) const
{
    if (hasLocation && (longitude < -180.0 || longitude > 180.0
                        || latitude < -90.0 || latitude > 90.0)) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("位置坐标无效"));
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT s.station_id, s.station_name, COALESCE(s.address, ''), "
            "s.longitude, s.latitude, s.price_per_kwh, COUNT(p.pile_id), "
            "COALESCE(SUM(CASE WHEN p.status='idle' THEN 1 ELSE 0 END), 0) "
            "FROM charging_stations s LEFT JOIN charging_piles p "
            "ON p.station_id=s.station_id GROUP BY s.station_id"))) {
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         QStringLiteral("充电站查询失败：%1")
                                             .arg(query.lastError().text()));
    }

    QList<QJsonObject> stations;
    while (query.next()) {
        QJsonObject station {
            {QStringLiteral("station_id"), query.value(0).toLongLong()},
            {QStringLiteral("station_name"), query.value(1).toString()},
            {QStringLiteral("address"), query.value(2).toString()},
            {QStringLiteral("price_per_kwh"), query.value(5).toDouble()},
            {QStringLiteral("total_piles"), query.value(6).toInt()},
            {QStringLiteral("idle_count"), query.value(7).toInt()}
        };
        const bool stationHasLocation = !query.value(3).isNull() && !query.value(4).isNull();
        if (stationHasLocation) {
            const double stationLongitude = query.value(3).toDouble();
            const double stationLatitude = query.value(4).toDouble();
            station.insert(QStringLiteral("longitude"), stationLongitude);
            station.insert(QStringLiteral("latitude"), stationLatitude);
            if (hasLocation) {
                station.insert(QStringLiteral("distance_km"),
                               haversine(latitude, longitude,
                                         stationLatitude, stationLongitude));
            } else {
                station.insert(QStringLiteral("distance_km"), QJsonValue::Null);
            }
        } else {
            station.insert(QStringLiteral("longitude"), QJsonValue::Null);
            station.insert(QStringLiteral("latitude"), QJsonValue::Null);
            station.insert(QStringLiteral("distance_km"), QJsonValue::Null);
        }
        stations.append(station);
    }
    std::sort(stations.begin(), stations.end(), [hasLocation](const QJsonObject &left,
                                                               const QJsonObject &right) {
        const QJsonValue leftDistance = left.value(QStringLiteral("distance_km"));
        const QJsonValue rightDistance = right.value(QStringLiteral("distance_km"));
        if (hasLocation && leftDistance.isDouble() != rightDistance.isDouble()) {
            return leftDistance.isDouble();
        }
        if (hasLocation && leftDistance.isDouble() && rightDistance.isDouble()
            && std::abs(leftDistance.toDouble() - rightDistance.toDouble()) > 0.000001) {
            return leftDistance.toDouble() < rightDistance.toDouble();
        }
        return left.value(QStringLiteral("station_id")).toInteger()
            < right.value(QStringLiteral("station_id")).toInteger();
    });
    QJsonArray array;
    for (const QJsonObject &station : stations) {
        array.append(station);
    }
    return Result<QJsonObject>::ok(QJsonObject {{QStringLiteral("stations"), array}});
}

Result<QJsonObject> StationService::queryPiles(QSqlDatabase &database, qint64 stationId) const
{
    if (stationId <= 0) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("站点编号无效"));
    }
    QSqlQuery stationQuery(database);
    stationQuery.prepare(QStringLiteral(
        "SELECT station_name, COALESCE(address, ''), price_per_kwh "
        "FROM charging_stations WHERE station_id=?"));
    stationQuery.addBindValue(stationId);
    if (!stationQuery.exec()) {
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         QStringLiteral("站点详情查询失败"));
    }
    if (!stationQuery.next()) {
        return Result<QJsonObject>::fail(ErrorCode::NotFound,
                                         QStringLiteral("充电站不存在"));
    }
    QJsonObject station {
        {QStringLiteral("station_id"), stationId},
        {QStringLiteral("station_name"), stationQuery.value(0).toString()},
        {QStringLiteral("address"), stationQuery.value(1).toString()},
        {QStringLiteral("price_per_kwh"), stationQuery.value(2).toDouble()}
    };

    QSqlQuery pileQuery(database);
    pileQuery.prepare(QStringLiteral(
        "SELECT pile_id, pile_number, pile_type, power_kw, status "
        "FROM charging_piles WHERE station_id=? ORDER BY pile_number ASC"));
    pileQuery.addBindValue(stationId);
    if (!pileQuery.exec()) {
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         QStringLiteral("充电桩查询失败"));
    }
    QJsonArray piles;
    int idle = 0;
    int inUse = 0;
    int fault = 0;
    while (pileQuery.next()) {
        const QString status = pileQuery.value(4).toString();
        idle += status == QStringLiteral("idle");
        inUse += status == QStringLiteral("in_use");
        fault += status == QStringLiteral("fault");
        piles.append(QJsonObject {
            {QStringLiteral("pile_id"), pileQuery.value(0).toLongLong()},
            {QStringLiteral("pile_number"), pileQuery.value(1).toString()},
            {QStringLiteral("pile_type"), pileQuery.value(2).toString()},
            {QStringLiteral("power_kw"), pileQuery.value(3).toDouble()},
            {QStringLiteral("status"), status}
        });
    }
    const int total = piles.size();
    const double onlineRate = total > 0 ? (idle + inUse) * 100.0 / total : 0.0;
    return Result<QJsonObject>::ok(QJsonObject {
        {QStringLiteral("station"), station},
        {QStringLiteral("piles"), piles},
        {QStringLiteral("stats"), QJsonObject {
             {QStringLiteral("total"), total},
             {QStringLiteral("idle"), idle},
             {QStringLiteral("in_use"), inUse},
             {QStringLiteral("fault"), fault},
             {QStringLiteral("online_rate"), onlineRate}
         }}
    });
}

} // namespace ev
