#pragma once

#include "common/result.h"

#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointF>
#include <QString>

#include <functional>

namespace ev {

// Server-side OpenStreetMap Nominatim adapter. QPointF stores longitude in x
// and latitude in y.
class MapApiAdapter final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const Result<QPointF> &)>;

    explicit MapApiAdapter(QObject *parent = nullptr);
    void geocode(const QString &address, Callback callback);

private:
    struct Request {
        QString address;
        Callback callback;
    };

    void processNext();

    QNetworkAccessManager network_;
    QHash<QString, QPointF> cache_;
    QList<Request> queue_;
    QString endpoint_;
    QByteArray userAgent_;
    bool requestInProgress_ = false;
};

} // namespace ev
