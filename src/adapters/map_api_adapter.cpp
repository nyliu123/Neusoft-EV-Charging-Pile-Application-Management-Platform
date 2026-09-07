#include "adapters/map_api_adapter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace ev {

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

void MapApiAdapter::geocode(const QString &address, Callback callback)
{
    const QString normalized = address.simplified();
    if (normalized.isEmpty() || normalized.size() > 200) {
        callback(Result<QPointF>::fail(ErrorCode::InvalidInput,
                                      QStringLiteral("地址不能为空且不能超过200个字符")));
        return;
    }
    const auto cached = cache_.constFind(normalized);
    if (cached != cache_.constEnd()) {
        callback(Result<QPointF>::ok(cached.value()));
        return;
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

    QString queryAddress = current.address;
    if (queryAddress == QStringLiteral("大连市高新区")
        || queryAddress == QStringLiteral("辽宁省大连市高新技术产业园区")) {
        // Nominatim has no administrative feature under the colloquial
        // "高新区" name; Ling Shui is the stable OSM locality for this area.
        queryAddress = QStringLiteral("凌水街道, 甘井子区, 大连市, 辽宁省");
    }

    QUrl url(endpoint_);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), queryAddress);
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
        Result<QPointF> result = Result<QPointF>::fail(
            ErrorCode::MapUnavailable, QStringLiteral("地图服务暂不可用"));
        if (reply->error() == QNetworkReply::NoError) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
            const QJsonArray matches = document.array();
            if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
                result = Result<QPointF>::fail(ErrorCode::MapUnavailable,
                                               QStringLiteral("地图服务响应无效"));
            } else if (matches.isEmpty()) {
                result = Result<QPointF>::fail(ErrorCode::NotFound,
                                               QStringLiteral("地址无法识别"));
            } else {
                const QJsonObject match = matches.first().toObject();
                bool longitudeOk = false;
                bool latitudeOk = false;
                const double longitude = match.value(QStringLiteral("lon")).toString()
                                             .toDouble(&longitudeOk);
                const double latitude = match.value(QStringLiteral("lat")).toString()
                                            .toDouble(&latitudeOk);
                if (longitudeOk && latitudeOk && longitude >= -180.0 && longitude <= 180.0
                    && latitude >= -90.0 && latitude <= 90.0) {
                    const QPointF location(longitude, latitude);
                    cache_.insert(current.address, location);
                    result = Result<QPointF>::ok(location);
                } else {
                    result = Result<QPointF>::fail(ErrorCode::MapUnavailable,
                                                   QStringLiteral("地图服务响应坐标无效"));
                }
            }
        }
        reply->deleteLater();
        current.callback(result);
        // Public Nominatim requires a maximum of one request per second.
        QTimer::singleShot(1000, this, [this] {
            requestInProgress_ = false;
            processNext();
        });
    });
}

} // namespace ev
