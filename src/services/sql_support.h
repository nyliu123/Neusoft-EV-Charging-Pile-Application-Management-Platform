#pragma once
#include "common/result.h"
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariant>
#include <functional>
#include <stdexcept>
#include <cmath>

namespace ev::sqlsupport {
using JsonResult = Result<QJsonObject>;
inline QSqlQuery query(QSqlDatabase &db, const QString &sql, const QVariantList &values = {}) {
    QSqlQuery q(db);
    if (!q.prepare(sql)) throw std::runtime_error("database prepare failed");
    for (const auto &v : values) q.addBindValue(v);
    if (!q.exec()) throw std::runtime_error("database query failed");
    return q;
}
inline QJsonObject row(const QSqlQuery &q) {
    QJsonObject result;
    for (int i = 0; i < q.record().count(); ++i)
        result.insert(q.record().fieldName(i), QJsonValue::fromVariant(q.value(i)));
    return result;
}
inline JsonResult guard(QSqlDatabase &db, bool transaction, const std::function<JsonResult()> &fn) {
    bool begun = false;
    try {
        if (transaction) { query(db, "BEGIN IMMEDIATE"); begun = true; }
        auto result = fn();
        if (begun) {
            if (result.success) {
                if (!db.commit()) { db.rollback(); return JsonResult::fail(ErrorCode::StorageError, QStringLiteral("保存失败，本次操作未生效")); }
            } else db.rollback();
        }
        return result;
    } catch (const std::exception &) {
        if (begun) db.rollback();
        return JsonResult::fail(ErrorCode::StorageError, QStringLiteral("数据操作失败，请稍后重试"));
    }
}
inline bool integer(const QJsonObject &object, const char *key, qint64 min, qint64 max) {
    const auto value = object.value(QLatin1String(key));
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && std::floor(n) == n && n >= min && n <= max;
}
}
