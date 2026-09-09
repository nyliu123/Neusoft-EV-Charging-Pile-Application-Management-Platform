#pragma once
#include "common/result.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>
namespace ev {
class ConsultApiAdapter final : public QObject {
public:
    using Callback=std::function<void(Result<QJsonObject>)>;
    explicit ConsultApiAdapter(QObject *parent=nullptr) : QObject(parent), manager_(this) {}
    void ask(const QString &question,const QJsonArray &sources,const QJsonArray &history,Callback callback);
    bool configured() const { return !qEnvironmentVariable("EV_AI_API_KEY").trimmed().isEmpty(); }
private:
    QNetworkAccessManager manager_;
};
}
