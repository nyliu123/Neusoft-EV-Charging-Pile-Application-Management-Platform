#pragma once
#include "common/result.h"
#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>

namespace ev {
class MembershipService final {
public:
    using JsonResult = Result<QJsonObject>;
    static qint64 now() { return QDateTime::currentSecsSinceEpoch(); }
    static qint64 addMonths(qint64 from, int months);
    JsonResult plans(QSqlDatabase &db, bool admin = false) const;
    JsonResult status(QSqlDatabase &db, qint64 userId, qint64 at = now()) const;
    // snapshot is read-only and can participate in the reservation transaction.
    JsonResult snapshot(QSqlDatabase &db, qint64 userId, qint64 at = now()) const;
    JsonResult purchase(QSqlDatabase &db, qint64 userId, const QJsonObject &params, qint64 at = now()) const;
    JsonResult setRenewal(QSqlDatabase &db, qint64 userId, const QJsonObject &params, qint64 at = now()) const;
    JsonResult updatePlan(QSqlDatabase &db, const QJsonObject &params) const;
    JsonResult processDue(QSqlDatabase &db, qint64 at = now()) const;
};
}
