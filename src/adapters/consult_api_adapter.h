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
    using ChunkCallback=std::function<void(const QString &)>;
    explicit ConsultApiAdapter(QObject *parent=nullptr) : QObject(parent), manager_(this) {}
    void planTool(const QString &question,const QJsonArray &history,
                  const QJsonObject &settings,Callback callback);
    void ask(const QString &question,const QJsonArray &sources,const QJsonArray &history,
             const QJsonObject &settings,const QJsonObject &databaseContext,
             ChunkCallback chunkCallback,Callback callback);
private:
    QNetworkAccessManager manager_;
};
}
