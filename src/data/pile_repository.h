#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace ev {

struct PileRecord {
    qint64 pileId = 0;
    qint64 stationId = 0;
    QString pileNumber;
    QString pileType;   // 'fast' | 'slow'
    double powerKw = 0.0;
    QString status;     // 'idle' | 'reserved' | 'in_use' | 'fault'
    int totalChargeCount = 0;
    double totalChargeDuration = 0.0;  // hours
};

struct PileStatusCount {
    QString status;
    int count = 0;
};

// UML-049: charging_piles table data access.
// Includes optimistic-locking status transitions (state machine).
class PileRepository final {
public:
    // ── Query methods ───────────────────────────────────────────────────
    // UML-019: piles in a station (for user client).
    Result<QVector<PileRecord>> findByStation(QSqlDatabase &database,
                                               qint64 stationId) const;
    Result<std::optional<PileRecord>> findById(QSqlDatabase &database,
                                                qint64 pileId) const;
    // UML-038: all piles (for admin client).
    Result<QVector<PileRecord>> listAll(QSqlDatabase &database) const;
    // UML-041: same as findByStation (admin station detail).
    Result<QVector<PileRecord>> listByStation(QSqlDatabase &database,
                                                qint64 stationId) const;
    // UML-037: pile status counts (dashboard).
    Result<QVector<PileStatusCount>> countByStatus(QSqlDatabase &database) const;

    // ── Write methods ──────────────────────────────────────────────────
    // Optimistic-locking status update.
    // Returns StateConflict if current status doesn't match oldStatus.
    // UML-025: idle → reserved
    // UML-027: reserved → in_use
    // UML-030: in_use → idle
    // UML-026: reserved → idle (cancel)
    // UML-039: fault → idle (admin restart)
    Result<bool> updateStatus(QSqlDatabase &database, qint64 pileId,
                               const QString &newStatus,
                               const QString &oldStatus) const;
    // UML-030: update cumulative stats after charge ends.
    Result<bool> updateStats(QSqlDatabase &database, qint64 pileId,
                              int addCount, double addDuration) const;

private:
    // State machine: returns true if old→new is a valid transition.
    static bool isValidTransition(const QString &oldStatus,
                                   const QString &newStatus);
};

} // namespace ev
