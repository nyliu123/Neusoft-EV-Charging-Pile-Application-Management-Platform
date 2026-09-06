#include "services/station_service.h"

#include <QtMath>
#include <algorithm>

namespace ev {

bool StationService::isValidCoordinate(double longitude, double latitude)
{
    return qIsFinite(longitude) && qIsFinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

double StationService::haversineKm(double firstLongitude, double firstLatitude,
                                   double secondLongitude, double secondLatitude)
{
    constexpr double earthRadiusKm = 6371.0;
    const double lat1 = qDegreesToRadians(firstLatitude);
    const double lat2 = qDegreesToRadians(secondLatitude);
    const double deltaLat = lat2 - lat1;
    const double deltaLon = qDegreesToRadians(secondLongitude - firstLongitude);
    const double sinLat = qSin(deltaLat / 2.0);
    const double sinLon = qSin(deltaLon / 2.0);
    const double a = sinLat * sinLat + qCos(lat1) * qCos(lat2) * sinLon * sinLon;
    const double clamped = qBound(0.0, a, 1.0);
    return earthRadiusKm * 2.0 * qAtan2(qSqrt(clamped), qSqrt(1.0 - clamped));
}

double StationService::onlineRate(const StationDetailRecord &detail)
{
    if (detail.station.totalPiles <= 0) {
        return 0.0;
    }
    int online = 0;
    for (const StationPileRecord &pile : detail.piles) {
        if (pile.status == QStringLiteral("idle")
            || pile.status == QStringLiteral("reserved")
            || pile.status == QStringLiteral("in_use")) {
            ++online;
        }
    }
    return static_cast<double>(online) / detail.station.totalPiles;
}

Result<QVector<StationListItem>> StationService::listStations(
    QSqlDatabase &database, std::optional<double> longitude,
    std::optional<double> latitude) const
{
    if (longitude.has_value() != latitude.has_value()) {
        return Result<QVector<StationListItem>>::fail(
            ErrorCode::InvalidInput, QStringLiteral("经纬度必须同时提供"));
    }
    if (longitude.has_value() && !isValidCoordinate(*longitude, *latitude)) {
        return Result<QVector<StationListItem>>::fail(
            ErrorCode::InvalidInput, QStringLiteral("经纬度超出有效范围"));
    }

    const auto records = repository_.listStations(database);
    if (!records.success) {
        return Result<QVector<StationListItem>>::fail(records.code, records.message);
    }

    QVector<StationListItem> items;
    items.reserve(records.data.size());
    for (const StationRecord &station : records.data) {
        StationListItem item;
        item.station = station;
        if (longitude.has_value() && station.hasLocation) {
            item.distanceKm = haversineKm(*longitude, *latitude,
                                          station.longitude, station.latitude);
        }
        items.append(item);
    }
    std::sort(items.begin(), items.end(), [](const StationListItem &left,
                                             const StationListItem &right) {
        if (left.distanceKm.has_value() != right.distanceKm.has_value()) {
            return left.distanceKm.has_value();
        }
        if (left.distanceKm.has_value()
            && !qFuzzyCompare(*left.distanceKm + 1.0, *right.distanceKm + 1.0)) {
            return *left.distanceKm < *right.distanceKm;
        }
        return left.station.stationId < right.station.stationId;
    });
    return Result<QVector<StationListItem>>::ok(items);
}

Result<StationDetailRecord> StationService::stationDetail(
    QSqlDatabase &database, qint64 stationId) const
{
    if (stationId <= 0) {
        return Result<StationDetailRecord>::fail(
            ErrorCode::InvalidInput, QStringLiteral("站点编号无效"));
    }
    const auto detail = repository_.findDetail(database, stationId);
    if (!detail.success) {
        return Result<StationDetailRecord>::fail(detail.code, detail.message);
    }
    if (!detail.data.has_value()) {
        return Result<StationDetailRecord>::fail(
            ErrorCode::NotFound, QStringLiteral("充电站不存在"));
    }
    return Result<StationDetailRecord>::ok(detail.data.value());
}

} // namespace ev
