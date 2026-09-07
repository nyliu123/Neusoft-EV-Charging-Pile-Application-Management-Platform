#include "adapters/map_api_adapter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <utility>

namespace ev {

namespace {

struct OfflineLocation {
    const char *keyword;
    double latitude;
    double longitude;
    const char *displayAddress;
};

// Keep classroom/VM demos usable when DNS or the public OSM endpoint is down.
// Addresses outside this small set are still resolved by OpenStreetMap.
const OfflineLocation kOfflineLocations[] = {
    {"软件园", 38.863650, 121.509605, "大连市甘井子区软件园"},
    {"高新区", 38.861000, 121.530000, "大连市高新区"},
    {"高新园", 38.861000, 121.530000, "大连市高新园区"},
    {"凌水", 38.861000, 121.530000, "大连市高新区凌水街道"},
    {"甘井子", 38.953300, 121.525500, "大连市甘井子区"},
    {"大连北站", 38.951800, 121.570020, "大连市甘井子区大连北站"}
};

bool validCoordinate(double longitude, double latitude)
{
    return longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

} // namespace

MapApiAdapter::MapApiAdapter(QObject *parent)
    : QObject(parent),
      endpoint_(qEnvironmentVariable(
          "EV_NOMINATIM_URL", "https://nominatim.openstreetmap.org/search")),
      userAgent_(qEnvironmentVariable(
          "EV_MAP_USER_AGENT",
          "Neusoft-EV-Charging-Platform/0.1 (educational desktop application)")
                     .toUtf8())
{
}

void MapApiAdapter::geocode(const QString &address, GeocodeCallback callback)
{
    const QString normalized = address.simplified();
    if (normalized.isEmpty() || normalized.size() > 200) {
        callback(Result<GeocodeResult>::fail(
            ErrorCode::InvalidInput,
            QStringLiteral("地址不能为空且不能超过200个字符")));
        return;
    }
    const auto cached = cache_.constFind(normalized);
    if (cached != cache_.constEnd()) {
        callback(Result<GeocodeResult>::ok(cached.value()));
        return;
    }
    for (const OfflineLocation &location : kOfflineLocations) {
        if (normalized.contains(QString::fromUtf8(location.keyword),
                                Qt::CaseInsensitive)) {
            GeocodeResult result;
            result.longitude = location.longitude;
            result.latitude = location.latitude;
            result.displayAddress = QString::fromUtf8(location.displayAddress);
            result.source = QStringLiteral("服务端离线教学坐标");
            result.confidence = 50;
            cache_.insert(normalized, result);
            callback(Result<GeocodeResult>::ok(result));
            return;
        }
    }
    queue_.append({normalized, std::move(callback)});
    processNext();
}

void MapApiAdapter::processNext()
{
    if (requestInProgress_ || queue_.isEmpty()) {
        return;
    }
    requestInProgress_ = true;
    const Request current = queue_.takeFirst();

    QUrl url(endpoint_);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), current.address);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("jsonv2"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("accept-language"), QStringLiteral("zh-CN"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent_);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(8000);
    QNetworkReply *reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, current] {
        Result<GeocodeResult> result = Result<GeocodeResult>::fail(
            ErrorCode::MapUnavailable,
            QStringLiteral("地图服务暂不可用，请检查网络或 DNS"));
        if (reply->error() == QNetworkReply::NoError) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
            const QJsonArray matches = document.array();
            if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
                result = Result<GeocodeResult>::fail(
                    ErrorCode::MapUnavailable, QStringLiteral("地图服务响应无效"));
            } else if (matches.isEmpty()) {
                result = Result<GeocodeResult>::fail(
                    ErrorCode::NotFound, QStringLiteral("地址无法识别"));
            } else {
                const QJsonObject match = matches.first().toObject();
                bool longitudeOk = false;
                bool latitudeOk = false;
                const double longitude = match.value(QStringLiteral("lon"))
                                             .toString().toDouble(&longitudeOk);
                const double latitude = match.value(QStringLiteral("lat"))
                                            .toString().toDouble(&latitudeOk);
                if (longitudeOk && latitudeOk
                    && validCoordinate(longitude, latitude)) {
                    GeocodeResult location;
                    location.longitude = longitude;
                    location.latitude = latitude;
                    location.displayAddress = match.value(
                        QStringLiteral("display_name")).toString(current.address);
                    location.source = QStringLiteral("OpenStreetMap Nominatim");
                    location.confidence = 100;
                    cache_.insert(current.address, location);
                    result = Result<GeocodeResult>::ok(location);
                } else {
                    result = Result<GeocodeResult>::fail(
                        ErrorCode::MapUnavailable,
                        QStringLiteral("地图服务响应坐标无效"));
                }
            }
        }
        reply->deleteLater();
        current.callback(result);
        QTimer::singleShot(1000, this, [this] {
            requestInProgress_ = false;
            processNext();
        });
    });
}

} // namespace ev
