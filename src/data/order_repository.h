#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace ev {

struct OrderRecord {
    qint64 orderId = 0;
    qint64 userId = 0;
    qint64 pileId = 0;
    qint64 stationId = 0;
    QString status;          // 'reserved'|'charging'|'pending_settlement'|'settled'|'cancelled'
    QString reserveTime;
    QString startTime;
    QString endTime;
    double chargeAmountKwh = 0.0;
    double pricePerKwh = 0.0;
    qint64 totalFeeCent = 0;   // fee in cents (integer cents for precision)
    // Joined fields for admin order list.
    QString userPhone;
    QString pileNumber;
    QString stationName;
};

struct SettledStats {
    double totalKwh = 0.0;
    qint64 totalRevenueCent = 0;  // in cents
    int totalCount = 0;
};

struct DailyRevenue {
    QString date;   // "2026-09-08"
    qint64 revenueCent = 0;
};

// UML-050: orders table data access.
class OrderRepository final {
public:
    // ── Query methods ───────────────────────────────────────────────────
    // UML-033: user's own orders.
    Result<QVector<OrderRecord>> findByUser(QSqlDatabase &database,
                                             qint64 userId) const;
    Result<std::optional<OrderRecord>> findById(QSqlDatabase &database,
                                                 qint64 orderId) const;
    // UML-025: check if user has a non-terminal order.
    Result<std::optional<OrderRecord>> checkPending(QSqlDatabase &database,
                                                     qint64 userId) const;
    // UML-046: all orders (admin, with joined fields).
    Result<QVector<OrderRecord>> listAll(QSqlDatabase &database) const;
    // UML-046: filtered orders (admin).
    Result<QVector<OrderRecord>> listFiltered(
        QSqlDatabase &database,
        const QString &status,
        qint64 stationId,
        const QString &startDate,
        const QString &endDate) const;
    // UML-035: settled order aggregate.
    Result<std::optional<SettledStats>> countSettled(QSqlDatabase &database) const;
    // UML-036: revenue by date.
    Result<QVector<DailyRevenue>> revenueByDate(
        QSqlDatabase &database,
        const QString &startDate,
        const QString &endDate) const;

    // ── Write methods ──────────────────────────────────────────────────
    // UML-025: create reservation order.
    Result<qint64> insertOrder(QSqlDatabase &database,
                                qint64 userId, qint64 pileId,
                                qint64 stationId, double pricePerKwh) const;
    // UML-027/030/031/026: status transitions.
    // 'charging' writes start_time; 'pending_settlement' writes end_time.
    Result<bool> updateStatus(QSqlDatabase &database, qint64 orderId,
                               const QString &newStatus) const;
    // UML-028: real-time charge data update.
    Result<bool> updateChargeData(QSqlDatabase &database, qint64 orderId,
                                   double kwh, qint64 feeCent) const;
    // UML-030/031: settle order with final data.
    Result<bool> settleOrder(QSqlDatabase &database, qint64 orderId,
                              double finalKwh, qint64 finalFeeCent) const;
};

} // namespace ev
