#include "adapters/map_api_adapter.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtMath>
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

const OfflineLocation kOfflineLocations[] = {
    {"软件园", 38.863650, 121.509605, "大连市甘井子区软件园"},
    {"甘井子", 38.953300, 121.525500, "大连市甘井子区"},
    {"高新园", 38.861000, 121.530000, "大连市高新园区"},
    {"大连北站", 38.951800, 121.570020, "大连市甘井子区大连北站"},
    {"海淀", 39.957300, 116.326900, "北京市海淀区"},
    {"朝阳", 39.997900, 116.487800, "北京市朝阳区"},
    {"东城", 39.928800, 116.416000, "北京市东城区"}
};

bool validCoordinate(double longitude, double latitude)
{
    return qIsFinite(longitude) && qIsFinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

} // namespace

MapApiAdapter::MapApiAdapter(QString apiKey, QString referer, QObject *parent)
    : QObject(parent), apiKey_(std::move(apiKey)), referer_(std::move(referer))
{
    apiKey_ = apiKey_.trimmed();
    referer_ = referer_.trimmed();
    if (referer_.isEmpty()) {
        referer_ = QStringLiteral("EV-CHARGING-DEMO");
    }
}

void MapApiAdapter::geocode(const QString &address, GeocodeCallback callback)
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 255) {
        callback(Result<GeocodeResult>::fail(
            ErrorCode::InvalidInput, QStringLiteral("请输入有效地址")));
        return;
    }

    if (apiKey_.isEmpty()) {
        for (const OfflineLocation &location : kOfflineLocations) {
            if (trimmed.contains(QString::fromUtf8(location.keyword), Qt::CaseInsensitive)) {
                GeocodeResult result;
                result.longitude = location.longitude;
                result.latitude = location.latitude;
                result.displayAddress = QString::fromUtf8(location.displayAddress);
                result.source = QStringLiteral("服务端离线教学坐标");
                result.confidence = 50;
                callback(Result<GeocodeResult>::ok(result));
                return;
            }
        }
        callback(Result<GeocodeResult>::fail(
            ErrorCode::MapUnavailable,
            QStringLiteral("地图服务未配置；可选择软件园、甘井子、高新园或大连北站使用教学坐标")));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), trimmed);
    query.addQueryItem(QStringLiteral("key"), apiKey_);
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    url.setQuery(query);
    performGet(url, [trimmed, callback = std::move(callback)](Result<QJsonObject> response) {
        if (!response.success) {
            callback(Result<GeocodeResult>::fail(response.code, response.message));
            return;
        }
        const QJsonObject resultObject = response.data.value(QStringLiteral("result")).toObject();
        const QJsonObject location = resultObject.value(QStringLiteral("location")).toObject();
        const double longitude = location.value(QStringLiteral("lng")).toDouble(999.0);
        const double latitude = location.value(QStringLiteral("lat")).toDouble(999.0);
        if (!validCoordinate(longitude, latitude)) {
            callback(Result<GeocodeResult>::fail(
                ErrorCode::MapUnavailable, QStringLiteral("地图服务未返回有效坐标")));
            return;
        }
        GeocodeResult result;
        result.longitude = longitude;
        result.latitude = latitude;
        result.displayAddress = resultObject.value(QStringLiteral("title")).toString(trimmed);
        result.source = QStringLiteral("腾讯地图 WebService");
        result.confidence = resultObject.value(QStringLiteral("confidence")).toInt();
        callback(Result<GeocodeResult>::ok(result));
    });
}

void MapApiAdapter::reverseGeocode(double longitude, double latitude,
                                   ReverseCallback callback)
{
    if (!validCoordinate(longitude, latitude)) {
        callback(Result<ReverseGeocodeResult>::fail(
            ErrorCode::InvalidInput, QStringLiteral("经纬度超出有效范围")));
        return;
    }
    if (apiKey_.isEmpty()) {
        callback(Result<ReverseGeocodeResult>::fail(
            ErrorCode::MapUnavailable, QStringLiteral("地图服务未配置")));
        return;
    }
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("location"),
        QStringLiteral("%1,%2").arg(latitude, 0, 'f', 6).arg(longitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("key"), apiKey_);
    query.addQueryItem(QStringLiteral("get_poi"), QStringLiteral("0"));
    url.setQuery(query);
    performGet(url, [callback = std::move(callback)](Result<QJsonObject> response) {
        if (!response.success) {
            callback(Result<ReverseGeocodeResult>::fail(response.code, response.message));
            return;
        }
        const QJsonObject resultObject = response.data.value(QStringLiteral("result")).toObject();
        ReverseGeocodeResult result;
        result.address = resultObject.value(QStringLiteral("address")).toString();
        result.source = QStringLiteral("腾讯地图 WebService");
        if (result.address.isEmpty()) {
            callback(Result<ReverseGeocodeResult>::fail(
                ErrorCode::MapUnavailable, QStringLiteral("地图服务未返回地址")));
            return;
        }
        callback(Result<ReverseGeocodeResult>::ok(result));
    });
}

void MapApiAdapter::route(double fromLongitude, double fromLatitude,
                          double toLongitude, double toLatitude,
                          const QString &mode, RouteCallback callback)
{
    if (!validCoordinate(fromLongitude, fromLatitude)
        || !validCoordinate(toLongitude, toLatitude)) {
        callback(Result<RouteResult>::fail(
            ErrorCode::InvalidInput, QStringLiteral("路线端点经纬度无效")));
        return;
    }
    const QString normalizedMode = mode == QStringLiteral("walking")
        ? QStringLiteral("walking") : QStringLiteral("driving");
    if (apiKey_.isEmpty()) {
        callback(Result<RouteResult>::fail(
            ErrorCode::MapUnavailable, QStringLiteral("地图服务未配置")));
        return;
    }
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/direction/v1/%1/")
                 .arg(normalizedMode));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"),
        QStringLiteral("%1,%2").arg(fromLatitude, 0, 'f', 6).arg(fromLongitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("to"),
        QStringLiteral("%1,%2").arg(toLatitude, 0, 'f', 6).arg(toLongitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("key"), apiKey_);
    url.setQuery(query);
    performGet(url, [normalizedMode, callback = std::move(callback)](
                        Result<QJsonObject> response) {
        if (!response.success) {
            callback(Result<RouteResult>::fail(response.code, response.message));
            return;
        }
        const QJsonArray routes = response.data.value(QStringLiteral("result")).toObject()
                                      .value(QStringLiteral("routes")).toArray();
        if (routes.isEmpty()) {
            callback(Result<RouteResult>::fail(
                ErrorCode::MapUnavailable, QStringLiteral("地图服务未返回可用路线")));
            return;
        }
        const QJsonObject first = routes.first().toObject();
        RouteResult result;
        result.distanceKm = first.value(QStringLiteral("distance")).toDouble() / 1000.0;
        result.durationMinutes = qCeil(first.value(QStringLiteral("duration")).toDouble() / 60.0);
        result.mode = normalizedMode;
        result.polyline = first.value(QStringLiteral("polyline")).toArray();
        callback(Result<RouteResult>::ok(result));
    });
}

void MapApiAdapter::performGet(const QUrl &url, JsonCallback callback)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EV-Charging-Teaching-Platform/0.2"));
    if (!referer_.isEmpty()) {
        request.setRawHeader("Referer", referer_.toUtf8());
    }
    QNetworkReply *reply = network_.get(request);
    QTimer::singleShot(8000, reply, [reply] {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [reply, callback = std::move(callback)]() mutable {
        const QByteArray body = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            callback(Result<QJsonObject>::fail(
                ErrorCode::MapUnavailable,
                QStringLiteral("腾讯地图请求失败：%1").arg(networkMessage)));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            callback(Result<QJsonObject>::fail(
                ErrorCode::MapUnavailable, QStringLiteral("腾讯地图响应无法解析")));
            return;
        }
        const QJsonObject root = document.object();
        if (root.value(QStringLiteral("status")).toInt(-1) != 0) {
            callback(Result<QJsonObject>::fail(
                ErrorCode::MapUnavailable,
                root.value(QStringLiteral("message")).toString(
                    QStringLiteral("腾讯地图未返回结果"))));
            return;
        }
        callback(Result<QJsonObject>::ok(root));
    });
}

} // namespace ev
