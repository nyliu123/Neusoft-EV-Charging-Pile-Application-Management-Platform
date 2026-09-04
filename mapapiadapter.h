#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

namespace evcs::server {

class MapApiAdapter final : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(bool, const QJsonObject &, const QString &)>;

    explicit MapApiAdapter(QObject *parent = nullptr);
    void configure(const QString &apiKey, const QString &referer);
    void geocode(const QString &address, Callback callback);
    QString apiKey() const;
    QString referer() const;

private:
    QNetworkAccessManager network_;
    QString apiKey_;
    QString referer_ = QStringLiteral("EVCS-DEMO");
};

} // namespace evcs::server
