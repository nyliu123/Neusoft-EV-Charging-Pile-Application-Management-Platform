#include "data/pile_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ev {

namespace {

PileRecord recordFromQuery(const QSqlQuery &query)
{
    PileRecord p;
    p.pileId       = query.value(0).toLongLong();
    p.stationId    = query.value(1).toLongLong();
    p.pileNumber   = query.value(2).toString();
    p.pileType     = query.value(3).toString();
    p.powerKw      = query.value(4).toDouble();
    p.status       = query.value(5).toString();
    p.totalChargeCount    = query.value(6).toInt();
    p.totalChargeDuration = query.value(7).toDouble();
    return p;
}

} // namespace

// ── UML-019: piles in a station ────────────────────────────────────────────

Result<QVector<PileRecord>> PileRepository::findByStation(
    QSqlDatabase &database, qint64 stationId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT pile_id, station_id, pile_number, pile_type, power_kw, "
        "status, total_charge_count, total_charge_duration "
        "FROM charging_piles WHERE station_id = ? ORDER BY pile_number"));
    query.addBindValue(stationId);
    if (!query.exec()) {
        return Result<QVector<PileRecord>>::fail(ErrorCode::StorageError,
                                                   query.lastError().text());
    }
    QVector<PileRecord> piles;
    while (query.next()) {
        piles.append(recordFromQuery(query));
    }
    return Result<QVector<PileRecord>>::ok(piles);
}

Result<std::optional<PileRecord>> PileRepository::findById(
    QSqlDatabase &database, qint64 pileId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT pile_id, station_id, pile_number, pile_type, power_kw, "
        "status, total_charge_count, total_charge_duration "
        "FROM charging_piles WHERE pile_id = ? LIMIT 1"));
    query.addBindValue(pileId);
    if (!query.exec()) {
        return Result<std::optional<PileRecord>>::fail(ErrorCode::StorageError,
                                                          query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<PileRecord>>::ok(std::nullopt);
    }
    return Result<std::optional<PileRecord>>::ok(recordFromQuery(query));
}

// ── UML-038: all piles ────────────────────────────────────────────────────

Result<QVector<PileRecord>> PileRepository::listAll(
    QSqlDatabase &database) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT pile_id, station_id, pile_number, pile_type, power_kw, "
            "status, total_charge_count, total_charge_duration "
            "FROM charging_piles ORDER BY station_id, pile_number"))) {
        return Result<QVector<PileRecord>>::fail(ErrorCode::StorageError,
                                                   query.lastError().text());
    }
    QVector<PileRecord> piles;
    while (query.next()) {
        piles.append(recordFromQuery(query));
    }
    return Result<QVector<PileRecord>>::ok(piles);
}

Result<QVector<PileRecord>> PileRepository::listByStation(
    QSqlDatabase &database, qint64 stationId) const
{
    return findByStation(database, stationId);
}

// ── UML-037: status counts ─────────────────────────────────────────────────

Result<QVector<PileStatusCount>> PileRepository::countByStatus(
    QSqlDatabase &database) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT status, COUNT(*) FROM charging_piles GROUP BY status"))) {
        return Result<QVector<PileStatusCount>>::fail(ErrorCode::StorageError,
                                                        query.lastError().text());
    }
    QVector<PileStatusCount> counts;
    while (query.next()) {
        PileStatusCount c;
        c.status = query.value(0).toString();
        c.count  = query.value(1).toInt();
        counts.append(c);
    }
    return Result<QVector<PileStatusCount>>::ok(counts);
}

// ── State machine validation ───────────────────────────────────────────────

bool PileRepository::isValidTransition(const QString &oldStatus,
                                        const QString &newStatus)
{
    // Legal transitions per UML-049:
    //   idle → reserved   (user reserves, UML-025)
    //   reserved → in_use (start charging, UML-027)
    //   reserved → idle   (cancel, UML-026)
    //   in_use → idle     (end charging, UML-030)
    //   fault → idle      (admin restart, UML-039)
    if (oldStatus == QStringLiteral("idle")
        && newStatus == QStringLiteral("reserved")) return true;
    if (oldStatus == QStringLiteral("reserved")
        && newStatus == QStringLiteral("in_use")) return true;
    if (oldStatus == QStringLiteral("reserved")
        && newStatus == QStringLiteral("idle")) return true;
    if (oldStatus == QStringLiteral("in_use")
        && newStatus == QStringLiteral("idle")) return true;
    if (oldStatus == QStringLiteral("fault")
        && newStatus == QStringLiteral("idle")) return true;
    return false;
}

// ── UML-025/027/030/026/039: optimistic-locking status update ───────────────

Result<bool> PileRepository::updateStatus(
    QSqlDatabase &database, qint64 pileId,
    const QString &newStatus, const QString &oldStatus) const
{
    if (!isValidTransition(oldStatus, newStatus)) {
        return Result<bool>::fail(ErrorCode::StateConflict,
                                   QStringLiteral("invalid_state_transition"));
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_piles SET status = ? "
        "WHERE pile_id = ? AND status = ?"));
    query.addBindValue(newStatus);
    query.addBindValue(pileId);
    query.addBindValue(oldStatus);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        // Status changed by another request (optimistic lock conflict).
        return Result<bool>::fail(ErrorCode::StateConflict,
                                   QStringLiteral("pile_status_changed"));
    }
    return Result<bool>::ok(true);
}

// ── UML-030: update cumulative stats ───────────────────────────────────────

Result<bool> PileRepository::updateStats(
    QSqlDatabase &database, qint64 pileId,
    int addCount, double addDuration) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_piles SET "
        "  total_charge_count = total_charge_count + ?, "
        "  total_charge_duration = total_charge_duration + ? "
        "WHERE pile_id = ?"));
    query.addBindValue(addCount);
    query.addBindValue(addDuration);
    query.addBindValue(pileId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("pile_not_found"));
    }
    return Result<bool>::ok(true);
}

} // namespace ev
