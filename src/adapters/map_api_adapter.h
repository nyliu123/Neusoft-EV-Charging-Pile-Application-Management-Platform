#pragma once

#include "common/result.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

namespace ev {

struct GeocodeResult {
    double longitude = 0.0;
    double latitude = 0.0;
    QString displayAddress;
    QString source;
    int confidence = 0;
};

struct ReverseGeocodeResult {
    QString address;
    QString source;
};

struct RouteResult {
    double distanceKm = 0.0;
    int durationMinutes = 0;
    QString mode;
    QJsonArray polyline;
};

class MapApiAdapter final : public QObject {
public:
    using GeocodeCallback = std::function<void(Result<GeocodeResult>)>;
    using ReverseCallback = std::function<void(Result<ReverseGeocodeResult>)>;
    using RouteCallback = std::function<void(Result<RouteResult>)>;

    explicit MapApiAdapter(QString apiKey = {}, QString referer = {},
                           QObject *parent = nullptr);

    void geocode(const QString &address, GeocodeCallback callback);
    void reverseGeocode(double longitude, double latitude, ReverseCallback callback);
    void route(double fromLongitude, double fromLatitude,
               double toLongitude, double toLatitude,
               const QString &mode, RouteCallback callback);

private:
    using JsonCallback = std::function<void(Result<QJsonObject>)>;
    void performGet(const QUrl &url, JsonCallback callback);

    QNetworkAccessManager network_;
    QString apiKey_;
    QString referer_;
};

} // namespace ev
