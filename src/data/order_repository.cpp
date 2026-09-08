#include "data/order_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>
#include <QList>

namespace ev {

namespace {

OrderRecord recordFromQuery(const QSqlQuery &query)
{
    OrderRecord o;
    o.orderId         = query.value(QStringLiteral("order_id")).toLongLong();
    o.userId          = query.value(QStringLiteral("user_id")).toLongLong();
    o.pileId          = query.value(QStringLiteral("pile_id")).toLongLong();
    o.stationId       = query.value(QStringLiteral("station_id")).toLongLong();
    o.status          = query.value(QStringLiteral("status")).toString();
    o.reserveTime     = query.value(QStringLiteral("reserve_time")).toString();
    o.startTime       = query.value(QStringLiteral("start_time")).toString();
    o.endTime         = query.value(QStringLiteral("end_time")).toString();
    o.chargeAmountKwh = query.value(QStringLiteral("charge_amount_kwh")).toDouble();
    o.pricePerKwh     = query.value(QStringLiteral("price_per_kwh")).toDouble();
    o.grossFeeCent = query.value("gross_fee_cent").toLongLong();
    o.membershipLevel = query.value("membership_level").toString();
    o.discountBps = query.value("discount_bps").toInt();
    o.membershipVersion = query.value("membership_version").toInt();
    // Store fee as cents.
    o.totalFeeCent    = static_cast<qint64>(
        std::round(query.value(QStringLiteral("total_fee")).toDouble() * 100.0));
    // Joined fields (may not exist in raw queries).
    const QSqlRecord rec = query.record();
    const int phoneIdx = rec.indexOf(QStringLiteral("user_phone"));
    if (phoneIdx >= 0) o.userPhone = query.value(phoneIdx).toString();
    const int pileNumIdx = rec.indexOf(QStringLiteral("pile_number"));
    if (pileNumIdx >= 0) o.pileNumber = query.value(pileNumIdx).toString();
    const int stationNameIdx = rec.indexOf(QStringLiteral("station_name"));
    if (stationNameIdx >= 0) o.stationName = query.value(stationNameIdx).toString();
    return o;
}

QString baseSelectSql()
{
    return QStringLiteral(
        "SELECT o.order_id, o.user_id, o.pile_id, o.station_id, o.status, "
        "o.reserve_time, o.start_time, o.end_time, "
        "o.charge_amount_kwh, o.price_per_kwh, o.total_fee, o.gross_fee_cent, o.membership_level, o.discount_bps, o.membership_version "
        "FROM orders o");
}

QString joinedSelectSql()
{
    return QStringLiteral(
        "SELECT o.order_id, o.user_id, o.pile_id, o.station_id, o.status, "
        "o.reserve_time, o.start_time, o.end_time, "
        "o.charge_amount_kwh, o.price_per_kwh, o.total_fee, o.gross_fee_cent, o.membership_level, o.discount_bps, o.membership_version, "
        "u.phone AS user_phone, p.pile_number AS pile_number, "
        "s.station_name AS station_name "
        "FROM orders o "
        "LEFT JOIN users u ON o.user_id = u.user_id "
        "LEFT JOIN charging_piles p ON o.pile_id = p.pile_id "
        "LEFT JOIN charging_stations s ON o.station_id = s.station_id");
}

} // namespace

// ── UML-033: user's own orders ─────────────────────────────────────────────

Result<QVector<OrderRecord>> OrderRepository::findByUser(
    QSqlDatabase &database, qint64 userId) const
{
    QSqlQuery query(database);
    query.prepare(joinedSelectSql()
                  + QStringLiteral(" WHERE o.user_id = ? "
                                   "ORDER BY o.reserve_time DESC, o.order_id DESC"));
    query.addBindValue(userId);
    if (!query.exec()) {
        return Result<QVector<OrderRecord>>::fail(ErrorCode::StorageError,
                                                    query.lastError().text());
    }
    QVector<OrderRecord> orders;
    while (query.next()) {
        orders.append(recordFromQuery(query));
    }
    return Result<QVector<OrderRecord>>::ok(orders);
}

Result<std::optional<OrderRecord>> OrderRepository::findById(
    QSqlDatabase &database, qint64 orderId) const
{
    QSqlQuery query(database);
    query.prepare(baseSelectSql()
                  + QStringLiteral(" WHERE o.order_id = ? LIMIT 1"));
    query.addBindValue(orderId);
    if (!query.exec()) {
        return Result<std::optional<OrderRecord>>::fail(ErrorCode::StorageError,
                                                          query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<OrderRecord>>::ok(std::nullopt);
    }
    return Result<std::optional<OrderRecord>>::ok(recordFromQuery(query));
}

// ── UML-025: check pending order ───────────────────────────────────────────

Result<std::optional<OrderRecord>> OrderRepository::checkPending(
    QSqlDatabase &database, qint64 userId) const
{
    QSqlQuery query(database);
    query.prepare(baseSelectSql()
                  + QStringLiteral(
                      " WHERE o.user_id = ? "
                      "AND o.status IN ('reserved','charging','pending_settlement') "
                      "ORDER BY o.reserve_time DESC LIMIT 1"));
    query.addBindValue(userId);
    if (!query.exec()) {
        return Result<std::optional<OrderRecord>>::fail(ErrorCode::StorageError,
                                                          query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<OrderRecord>>::ok(std::nullopt);
    }
    return Result<std::optional<OrderRecord>>::ok(recordFromQuery(query));
}

// ── UML-046: all orders (admin) ────────────────────────────────────────────

Result<QVector<OrderRecord>> OrderRepository::listAll(QSqlDatabase &database) const
{
    QSqlQuery query(database);
    if (!query.exec(joinedSelectSql()
                    + QStringLiteral(" ORDER BY o.reserve_time DESC"))) {
        return Result<QVector<OrderRecord>>::fail(ErrorCode::StorageError,
                                                    query.lastError().text());
    }
    QVector<OrderRecord> orders;
    while (query.next()) {
        orders.append(recordFromQuery(query));
    }
    return Result<QVector<OrderRecord>>::ok(orders);
}

Result<QVector<OrderRecord>> OrderRepository::listFiltered(
    QSqlDatabase &database,
    const QString &status,
    qint64 stationId,
    const QString &startDate,
    const QString &endDate) const
{
    QString sql = joinedSelectSql() + QStringLiteral(" WHERE 1=1");
    QList<QVariant> binds;
    if (!status.isEmpty()) {
        sql += QStringLiteral(" AND o.status = ?");
        binds.append(status);
    }
    if (stationId > 0) {
        sql += QStringLiteral(" AND o.station_id = ?");
        binds.append(stationId);
    }
    if (!startDate.isEmpty()) {
        sql += QStringLiteral(" AND o.reserve_time >= ?");
        binds.append(startDate);
    }
    if (!endDate.isEmpty()) {
        sql += QStringLiteral(" AND o.reserve_time <= ?");
        binds.append(endDate);
    }
    sql += QStringLiteral(" ORDER BY o.reserve_time DESC");

    QSqlQuery query(database);
    query.prepare(sql);
    for (const auto &v : binds) query.addBindValue(v);
    if (!query.exec()) {
        return Result<QVector<OrderRecord>>::fail(ErrorCode::StorageError,
                                                    query.lastError().text());
    }
    QVector<OrderRecord> orders;
    while (query.next()) {
        orders.append(recordFromQuery(query));
    }
    return Result<QVector<OrderRecord>>::ok(orders);
}

// ── UML-035: settled stats ──────────────────────────────────────────────────

Result<std::optional<SettledStats>> OrderRepository::countSettled(
    QSqlDatabase &database) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT COALESCE(SUM(charge_amount_kwh), 0), "
            "COALESCE(ROUND(SUM(total_fee) * 100), 0), COUNT(*) "
            "FROM orders WHERE status = 'settled'")) || !query.next()) {
        return Result<std::optional<SettledStats>>::fail(ErrorCode::StorageError,
                                                           query.lastError().text());
    }
    SettledStats s;
    s.totalKwh      = query.value(0).toDouble();
    s.totalRevenueCent = query.value(1).toLongLong();
    s.totalCount    = query.value(2).toInt();
    return Result<std::optional<SettledStats>>::ok(s);
}

// ── UML-036: revenue by date ───────────────────────────────────────────────

Result<QVector<DailyRevenue>> OrderRepository::revenueByDate(
    QSqlDatabase &database,
    const QString &startDate,
    const QString &endDate) const
{
    QString sql = QStringLiteral(
        "SELECT DATE(end_time) AS d, "
        "COALESCE(ROUND(SUM(total_fee) * 100), 0) "
        "FROM orders WHERE status = 'settled' AND end_time IS NOT NULL");
    QList<QVariant> binds;
    if (!startDate.isEmpty()) {
        sql += QStringLiteral(" AND DATE(end_time) >= ?");
        binds.append(startDate);
    }
    if (!endDate.isEmpty()) {
        sql += QStringLiteral(" AND DATE(end_time) <= ?");
        binds.append(endDate);
    }
    sql += QStringLiteral(" GROUP BY d ORDER BY d ASC");

    QSqlQuery query(database);
    query.prepare(sql);
    for (const auto &v : binds) query.addBindValue(v);
    if (!query.exec()) {
        return Result<QVector<DailyRevenue>>::fail(ErrorCode::StorageError,
                                                      query.lastError().text());
    }
    QVector<DailyRevenue> out;
    while (query.next()) {
        DailyRevenue r;
        r.date       = query.value(0).toString();
        r.revenueCent = query.value(1).toLongLong();
        out.append(r);
    }
    return Result<QVector<DailyRevenue>>::ok(out);
}

// ── UML-025: insert order ───────────────────────────────────────────────────

Result<qint64> OrderRepository::insertOrder(
    QSqlDatabase &database, qint64 userId, qint64 pileId,
    qint64 stationId, double pricePerKwh, const QString &membershipLevel,
    int discountBps, int membershipVersion) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO orders "
        "(user_id, pile_id, station_id, status, reserve_time, price_per_kwh, membership_level, discount_bps, membership_version) "
        "VALUES (?, ?, ?, 'reserved', datetime('now','localtime'), ?, ?, ?, ?)"));
    query.addBindValue(userId);
    query.addBindValue(pileId);
    query.addBindValue(stationId);
    query.addBindValue(pricePerKwh);
    query.addBindValue(membershipLevel);
    query.addBindValue(discountBps);
    query.addBindValue(membershipVersion);
    if (!query.exec()) {
        return Result<qint64>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<qint64>::ok(query.lastInsertId().toLongLong());
}

// ── UML-027/030/031/026: status update ──────────────────────────────────────

Result<bool> OrderRepository::updateStatus(
    QSqlDatabase &database, qint64 orderId, const QString &newStatus) const
{
    QSqlQuery query(database);
    if (newStatus == QStringLiteral("charging")) {
        query.prepare(QStringLiteral(
            "UPDATE orders SET status = ?, start_time = datetime('now','localtime') "
            "WHERE order_id = ?"));
    } else if (newStatus == QStringLiteral("pending_settlement")) {
        query.prepare(QStringLiteral(
            "UPDATE orders SET status = ?, end_time = datetime('now','localtime') "
            "WHERE order_id = ?"));
    } else {
        query.prepare(QStringLiteral(
            "UPDATE orders SET status = ? WHERE order_id = ?"));
    }
    query.addBindValue(newStatus);
    query.addBindValue(orderId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("order_not_found"));
    }
    return Result<bool>::ok(true);
}

// ── UML-028: real-time charge data ─────────────────────────────────────────

Result<bool> OrderRepository::updateChargeData(
    QSqlDatabase &database, qint64 orderId,
    double kwh, qint64 feeCent, qint64 grossFeeCent) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE orders SET charge_amount_kwh = ?, total_fee = ? / 100.0, gross_fee_cent = ? "
        "WHERE order_id = ?"));
    query.addBindValue(kwh);
    query.addBindValue(feeCent);
    query.addBindValue(grossFeeCent < 0 ? feeCent : grossFeeCent);
    query.addBindValue(orderId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("order_not_found"));
    }
    return Result<bool>::ok(true);
}

// ── UML-030/031: settle order ──────────────────────────────────────────────

Result<bool> OrderRepository::settleOrder(
    QSqlDatabase &database, qint64 orderId,
    double finalKwh, qint64 finalFeeCent, qint64 grossFeeCent) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE orders SET "
        "  charge_amount_kwh = ?, total_fee = ? / 100.0, gross_fee_cent = ?, "
        "  status = 'settled', end_time = datetime('now','localtime') "
        "WHERE order_id = ?"));
    query.addBindValue(finalKwh);
    query.addBindValue(finalFeeCent);
    query.addBindValue(grossFeeCent < 0 ? finalFeeCent : grossFeeCent);
    query.addBindValue(orderId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("order_not_found"));
    }
    return Result<bool>::ok(true);
}

} // namespace ev
