#pragma once
#include "common/result.h"
#include <QJsonObject>
#include <QSqlDatabase>
namespace ev {
class KnowledgeService final {
public:
    Result<QJsonObject> list(QSqlDatabase &db) const;
    Result<QJsonObject> save(QSqlDatabase &db, const QJsonObject &params) const;
    Result<QJsonObject> publish(QSqlDatabase &db, const QJsonObject &params, bool active) const;
    Result<QJsonObject> retrieve(QSqlDatabase &db, const QString &question) const;
};
}
