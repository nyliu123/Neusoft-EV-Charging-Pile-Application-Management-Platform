#pragma once

#include "common/result.h"
#include "data/order_repository.h"
#include "data/pile_repository.h"
#include "data/station_repository.h"
#include "data/user_repository.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace ev {

// Live charge data derived from wall-clock elapsed time (server driven,
// UML-029/030: kwh = power_kw * elapsed_sec / 3600, fee = kwh * price).
struct ChargeSnapshot {
    qint64 elapsedSec = 0;
    double kwh = 0.0;
    qint64 feeCent = 0;
    double progressPercent = 0.0;   // 0..100, assumes a 50 kWh full charge
};

struct PendingOrderInfo {
    OrderRecord order;
    QString pileNumber;
    QString pileType;
    double powerKw = 0.0;
    QString stationName;
    std::optional<ChargeSnapshot> liveSnapshot;   // only for status == charging
};

struct PileCheckInfo {
    bool available = false;
    QString reason;
    PileRecord pile;
    QString stationName;
    double pricePerKwh = 0.0;
};

struct ReserveOutcome {
    qint64 orderId = 0;
    PileRecord pile;
    QString stationName;
    double pricePerKwh = 0.0;
};

struct StartedCharge {
    OrderRecord order;       // status = charging, start_time filled
    double powerKw = 0.0;
};

struct SettleOutcome {
    bool settled = false;    // false => pending_settlement (insufficient balance)
    qint64 orderId = 0;
    double totalKwh = 0.0;
    qint64 totalFeeCent = 0;
    qint64 balanceCent = 0;  // new balance after deduction, or current balance
    qint64 shortfallCent = 0;
};

// UML-025~032: charging business flow on top of the DAL repositories.
// reserve / startCharge / endCharge / cancel drive one database transaction
// each so order, pile and balance writes stay atomic (UML-032 consistency);
// pile concurrency is guarded by the repository's optimistic-locking status
// transitions.
class ChargeService final {
public:
    // Timestamps are stored as datetime('now','localtime') strings.
    static QDateTime parseDbDateTime(const QString &value);
    static Result<ChargeSnapshot> computeChargeData(const QDateTime &startTime,
                                                    double powerKw,
                                                    double pricePerKwh);

    // UML-025: latest non-terminal order for the user, enriched for display.
    Result<std::optional<PendingOrderInfo>> checkPending(QSqlDatabase &database,
                                                          qint64 userId) const;
    // UML-026: optimistic availability re-check right before reserving.
    Result<PileCheckInfo> checkPile(QSqlDatabase &database, qint64 pileId) const;
    // UML-027: pile idle→reserved + order insert (price snapshot) in one tx.
    Result<ReserveOutcome> reserve(QSqlDatabase &database, qint64 userId,
                                   qint64 pileId) const;
    // UML-028: order reserved→charging (start_time) + pile reserved→in_use.
    // userId is the session owner: orders belonging to other users are rejected.
    Result<StartedCharge> startCharge(QSqlDatabase &database, qint64 userId,
                                      qint64 orderId) const;
    // UML-031: settle when balance is sufficient (order settled + pile idle +
    // balance deduction + pile stats), otherwise park the order in
    // pending_settlement and keep the pile in use.
    Result<SettleOutcome> endCharge(QSqlDatabase &database, qint64 userId,
                                    qint64 orderId) const;
    // Release a reserved order: order cancelled + pile reserved→idle.
    Result<bool> cancel(QSqlDatabase &database, qint64 userId, qint64 orderId) const;

private:
    OrderRepository orderRepository_;
    PileRepository pileRepository_;
    StationRepository stationRepository_;
    UserRepository userRepository_;
};

} // namespace ev
