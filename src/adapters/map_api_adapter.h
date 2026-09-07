#pragma once

#include "common/result.h"

#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

namespace ev {

struct GeocodeResult {
    double longitude = 0.0;
    double latitude = 0.0;
    QString displayAddress;
    QString source;
    int confidence = 0;
};

// Server-side OpenStreetMap Nominatim adapter. Requests are serialized to
// respect Nominatim's public one-request-per-second usage policy.
class MapApiAdapter final : public QObject {
public:
    using GeocodeCallback = std::function<void(Result<GeocodeResult>)>;

    explicit MapApiAdapter(QObject *parent = nullptr);
    void geocode(const QString &address, GeocodeCallback callback);

private:
    struct Request {
        QString address;
        GeocodeCallback callback;
    };

    void processNext();

    QNetworkAccessManager network_;
    QHash<QString, GeocodeResult> cache_;
    QList<Request> queue_;
    QString endpoint_;
    QByteArray userAgent_;
    bool requestInProgress_ = false;
};

} // namespace ev
