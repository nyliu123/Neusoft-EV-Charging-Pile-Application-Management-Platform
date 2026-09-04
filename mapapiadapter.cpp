#include "mapapiadapter.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace evcs::server {

MapApiAdapter::MapApiAdapter(QObject *parent)
    : QObject(parent)
{
}

void MapApiAdapter::configure(const QString &apiKey, const QString &referer)
{
    apiKey_ = apiKey.trimmed();
    if (!referer.trimmed().isEmpty()) referer_ = referer.trimmed();
}

QString MapApiAdapter::apiKey() const { return apiKey_; }
QString MapApiAdapter::referer() const { return referer_; }

void MapApiAdapter::geocode(const QString &address, Callback callback)
{
    struct OfflineLocation { const char *keyword; double latitude; double longitude; const char *title; };
    static const OfflineLocation offline[] = {
        {"中关村", 39.9573, 116.3269, "北京市海淀区中关村"},
        {"海淀", 39.9573, 116.3269, "北京市海淀区"},
        {"亦庄", 39.7942, 116.5068, "北京市大兴区亦庄"},
        {"大兴", 39.7942, 116.5068, "北京市大兴区"},
        {"望京", 39.9979, 116.4878, "北京市朝阳区望京"},
        {"朝阳", 39.9979, 116.4878, "北京市朝阳区"},
        {"东城", 39.9288, 116.4160, "北京市东城区"}
    };
    if (address.trimmed().isEmpty()) {
        callback(false, {}, QStringLiteral("地址不能为空"));
        return;
    }
    if (apiKey_.isEmpty()) {
        for (const OfflineLocation &candidate : offline) {
            if (address.contains(QString::fromUtf8(candidate.keyword), Qt::CaseInsensitive)) {
                callback(true, {
                    {QStringLiteral("latitude"), candidate.latitude},
                    {QStringLiteral("longitude"), candidate.longitude},
                    {QStringLiteral("title"), QString::fromUtf8(candidate.title)},
                    {QStringLiteral("source"), QStringLiteral("服务端离线教学坐标")},
                    {QStringLiteral("mapKey"), QString{}},
                    {QStringLiteral("mapReferer"), referer_}
                }, {});
                return;
            }
        }
        callback(false, {}, QStringLiteral(
            "服务端未配置腾讯位置服务 Key；可输入中关村、海淀、亦庄、大兴、望京、朝阳或东城使用离线坐标"));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    query.addQueryItem(QStringLiteral("key"), apiKey_);
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EVCS-Teaching-Platform/1.2"));
    QNetworkReply *reply = network_.get(request);
    QTimer::singleShot(8000, reply, [reply] { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, address, callback] {
        const QByteArray body = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            callback(false, {}, QStringLiteral("腾讯地图请求失败：%1").arg(networkMessage));
            return;
        }
        QJsonParseError parseError;
        const QJsonObject root = QJsonDocument::fromJson(body, &parseError).object();
        if (parseError.error != QJsonParseError::NoError
            || root.value(QStringLiteral("status")).toInt(-1) != 0) {
            callback(false, {}, root.value(QStringLiteral("message")).toString(
                QStringLiteral("腾讯地图响应无法解析")));
            return;
        }
        const QJsonObject result = root.value(QStringLiteral("result")).toObject();
        const QJsonObject location = result.value(QStringLiteral("location")).toObject();
        const double latitude = location.value(QStringLiteral("lat")).toDouble(999.0);
        const double longitude = location.value(QStringLiteral("lng")).toDouble(999.0);
        if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
            callback(false, {}, QStringLiteral("腾讯地图未返回有效坐标"));
            return;
        }
        callback(true, {
            {QStringLiteral("latitude"), latitude},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("title"), result.value(QStringLiteral("title")).toString(address)},
            {QStringLiteral("source"), QStringLiteral("腾讯地图 WebService（服务端）")},
            {QStringLiteral("mapKey"), apiKey_},
            {QStringLiteral("mapReferer"), referer_}
        }, {});
    });
}

} // namespace evcs::server
