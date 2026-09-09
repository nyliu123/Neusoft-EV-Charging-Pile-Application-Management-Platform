#include "services/charge_service.h"

#include "services/fee_calculator.h"
#include "services/membership_service.h"

#include <QtGlobal>

#include <cmath>

namespace ev {

namespace {

// Design assumption (UML-029): a full charge is 50 kWh.
constexpr double kFullChargeKwh = 50.0;

QString transitionFailureMessage(const Result<bool> &result)
{
    if (result.message == QStringLiteral("pile_status_changed")) {
        return QStringLiteral("该充电桩刚被其他用户预约，请选择其他桩");
    }
    return QStringLiteral("充电桩状态已变化，请刷新后重试");
}

} // namespace

QDateTime ChargeService::parseDbDateTime(const QString &value)
{
    return QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
}

Result<ChargeSnapshot> ChargeService::computeChargeData(const QDateTime &startTime,
                                                        double powerKw,
                                                        double pricePerKwh, int discountBps)
{
    if (!startTime.isValid() || powerKw <= 0.0 || pricePerKwh <= 0.0) {
        return Result<ChargeSnapshot>::fail(ErrorCode::InvalidInput,
                                            QStringLiteral("invalid charge parameters"));
    }
    ChargeSnapshot snapshot;
    snapshot.elapsedSec = qMax<qint64>(0, startTime.secsTo(QDateTime::currentDateTime()));
    snapshot.kwh = powerKw * static_cast<double>(snapshot.elapsedSec) / 3600.0;
    const qint64 priceCent = static_cast<qint64>(std::llround(pricePerKwh * 100.0));
    const auto fee = FeeCalculator::calculate(
        static_cast<long double>(snapshot.kwh), priceCent, discountBps);
    if (!fee.success) {
        return Result<ChargeSnapshot>::fail(fee.code, fee.message);
    }
    snapshot.feeCent = fee.data.netCent;
    snapshot.grossFeeCent = fee.data.grossCent;
    snapshot.progressPercent = qMin(100.0, snapshot.kwh / kFullChargeKwh * 100.0);
    return Result<ChargeSnapshot>::ok(snapshot);
}

Result<std::optional<PendingOrderInfo>> ChargeService::checkPending(
    QSqlDatabase &database, qint64 userId) const
{
    const auto pending = orderRepository_.checkPending(database, userId);
    if (!pending.success) {
        return Result<std::optional<PendingOrderInfo>>::fail(pending.code, pending.message);
    }
    if (!pending.data.has_value()) {
        return Result<std::optional<PendingOrderInfo>>::ok(std::nullopt);
    }

    PendingOrderInfo info;
    info.order = *pending.data;

    const auto pile = pileRepository_.findById(database, info.order.pileId);
    if (pile.success && pile.data.has_value()) {
        info.pileNumber = pile.data->pileNumber;
        info.pileType = pile.data->pileType;
        info.powerKw = pile.data->powerKw;
    }
    const auto station = stationRepository_.findDetail(database, info.order.stationId);
    if (station.success && station.data.has_value()) {
        info.stationName = station.data->station.stationName;
    }

    if (info.order.status == QStringLiteral("charging")) {
        const QDateTime start = parseDbDateTime(info.order.startTime);
        const auto snapshot = computeChargeData(start, info.powerKw, info.order.pricePerKwh, info.order.discountBps);
        if (snapshot.success) {
            info.liveSnapshot = snapshot.data;
        }
    }
    return Result<std::optional<PendingOrderInfo>>::ok(info);
}

Result<PileCheckInfo> ChargeService::checkPile(QSqlDatabase &database,
                                               qint64 pileId) const
{
    const auto pile = pileRepository_.findById(database, pileId);
    if (!pile.success) {
        return Result<PileCheckInfo>::fail(pile.code, pile.message);
    }
    if (!pile.data.has_value()) {
        return Result<PileCheckInfo>::fail(ErrorCode::NotFound,
                                           QStringLiteral("充电桩不存在"));
    }

    PileCheckInfo info;
    info.pile = *pile.data;
    const auto station = stationRepository_.findDetail(database, info.pile.stationId);
    if (station.success && station.data.has_value()) {
        info.stationName = station.data->station.stationName;
        info.pricePerKwh = station.data->station.pricePerKwh;
    }

    if (info.pile.status == QStringLiteral("idle")) {
        info.available = true;
    } else if (info.pile.status == QStringLiteral("fault")) {
        info.available = false;
        info.reason = QStringLiteral("该充电桩当前故障，请选择其他桩");
    } else {
        info.available = false;
        info.reason = QStringLiteral("该充电桩已被占用，请选择其他桩");
    }
    return Result<PileCheckInfo>::ok(info);
}

Result<ReserveOutcome> ChargeService::reserve(QSqlDatabase &database, qint64 userId,
                                              qint64 pileId) const
{
    // Double check the single-active-order rule (UML-027 step 5).
    const auto pending = orderRepository_.checkPending(database, userId);
    if (!pending.success) {
        return Result<ReserveOutcome>::fail(pending.code, pending.message);
    }
    if (pending.data.has_value()) {
        return Result<ReserveOutcome>::fail(ErrorCode::RequestConflict,
                                            QStringLiteral("您有未完成的充电订单，请先处理"));
    }

    const auto pile = pileRepository_.findById(database, pileId);
    if (!pile.success) {
        return Result<ReserveOutcome>::fail(pile.code, pile.message);
    }
    if (!pile.data.has_value()) {
        return Result<ReserveOutcome>::fail(ErrorCode::NotFound,
                                            QStringLiteral("充电桩不存在"));
    }
    if (pile.data->status != QStringLiteral("idle")) {
        return Result<ReserveOutcome>::fail(ErrorCode::StateConflict,
                                            QStringLiteral("该充电桩刚被其他用户预约，请选择其他桩"));
    }

    const auto station = stationRepository_.findDetail(database, pile.data->stationId);
    if (!station.success) {
        return Result<ReserveOutcome>::fail(station.code, station.message);
    }
    if (!station.data.has_value() || station.data->station.pricePerKwh <= 0.0) {
        return Result<ReserveOutcome>::fail(ErrorCode::NotFound,
                                            QStringLiteral("充电站信息缺失，无法预约"));
    }

    if (!database.transaction()) {
        return Result<ReserveOutcome>::fail(ErrorCode::StorageError,
                                            QStringLiteral("cannot begin reserve transaction"));
    }
    // Optimistic lock: fails with StateConflict if another user won the race.
    const auto locked = pileRepository_.updateStatus(
        database, pileId, QStringLiteral("reserved"), QStringLiteral("idle"));
    if (!locked.success) {
        database.rollback();
        return Result<ReserveOutcome>::fail(locked.code, transitionFailureMessage(locked));
    }
    const auto membership = MembershipService().snapshot(database, userId);
    if (!membership.success) {
        database.rollback();
        return Result<ReserveOutcome>::fail(membership.code, membership.message);
    }
    const auto order = orderRepository_.insertOrder(
        database, userId, pileId, pile.data->stationId,
        station.data->station.pricePerKwh, membership.data.value("level").toString(),
        membership.data.value("discount_bps").toInt(), membership.data.value("version").toInt());
    if (!order.success) {
        database.rollback();
        return Result<ReserveOutcome>::fail(order.code, order.message);
    }
    if (!database.commit()) {
        database.rollback();
        return Result<ReserveOutcome>::fail(ErrorCode::StorageError,
                                            QStringLiteral("cannot commit reserve transaction"));
    }

    ReserveOutcome outcome;
    outcome.orderId = order.data;
    outcome.membershipLevel = membership.data.value("level").toString();
    outcome.discountBps = membership.data.value("discount_bps").toInt();
    outcome.pile = *pile.data;
    outcome.stationName = station.data->station.stationName;
    outcome.pricePerKwh = station.data->station.pricePerKwh;
    return Result<ReserveOutcome>::ok(outcome);
}

Result<StartedCharge> ChargeService::startCharge(QSqlDatabase &database, qint64 userId,
                                                 qint64 orderId) const
{
    const auto existing = orderRepository_.findById(database, orderId);
    if (!existing.success) {
        return Result<StartedCharge>::fail(existing.code, existing.message);
    }
    if (!existing.data.has_value()) {
        return Result<StartedCharge>::fail(ErrorCode::NotFound,
                                           QStringLiteral("订单不存在"));
    }
    if (existing.data->userId != userId) {
        return Result<StartedCharge>::fail(ErrorCode::NotFound,
                                           QStringLiteral("订单不存在"));
    }
    if (existing.data->status != QStringLiteral("reserved")) {
        return Result<StartedCharge>::fail(ErrorCode::StateConflict,
                                           QStringLiteral("订单状态异常，请重新操作"));
    }

    const auto pile = pileRepository_.findById(database, existing.data->pileId);
    if (!pile.success) {
        return Result<StartedCharge>::fail(pile.code, pile.message);
    }
    if (!pile.data.has_value() || pile.data->status != QStringLiteral("reserved")) {
        return Result<StartedCharge>::fail(ErrorCode::StateConflict,
                                           QStringLiteral("充电桩状态异常，请联系管理员"));
    }

    if (!database.transaction()) {
        return Result<StartedCharge>::fail(ErrorCode::StorageError,
                                           QStringLiteral("cannot begin start transaction"));
    }
    const auto orderUpdate = orderRepository_.updateStatus(
        database, orderId, QStringLiteral("charging"));
    if (!orderUpdate.success) {
        database.rollback();
        return Result<StartedCharge>::fail(orderUpdate.code, orderUpdate.message);
    }
    const auto pileUpdate = pileRepository_.updateStatus(
        database, existing.data->pileId, QStringLiteral("in_use"),
        QStringLiteral("reserved"));
    if (!pileUpdate.success) {
        database.rollback();
        return Result<StartedCharge>::fail(pileUpdate.code,
                                           QStringLiteral("充电桩状态异常，请联系管理员"));
    }
    if (!database.commit()) {
        database.rollback();
        return Result<StartedCharge>::fail(ErrorCode::StorageError,
                                           QStringLiteral("cannot commit start transaction"));
    }

    // Re-read so the caller gets the persisted start_time.
    const auto started = orderRepository_.findById(database, orderId);
    if (!started.success || !started.data.has_value()) {
        return Result<StartedCharge>::fail(ErrorCode::StorageError,
                                           QStringLiteral("cannot reload started order"));
    }
    StartedCharge charge;
    charge.order = *started.data;
    charge.powerKw = pile.data->powerKw;
    return Result<StartedCharge>::ok(charge);
}

Result<SettleOutcome> ChargeService::endCharge(QSqlDatabase &database, qint64 userId,
                                               qint64 orderId) const
{
    const auto existing = orderRepository_.findById(database, orderId);
    if (!existing.success) {
        return Result<SettleOutcome>::fail(existing.code, existing.message);
    }
    if (!existing.data.has_value()) {
        return Result<SettleOutcome>::fail(ErrorCode::NotFound,
                                           QStringLiteral("订单不存在"));
    }
    if (existing.data->userId != userId) {
        return Result<SettleOutcome>::fail(ErrorCode::NotFound,
                                           QStringLiteral("订单不存在"));
    }
    const OrderRecord &order = *existing.data;
    // end_charge also re-settles a pending_settlement order after recharge.
    if (order.status != QStringLiteral("charging")
        && order.status != QStringLiteral("pending_settlement")) {
        return Result<SettleOutcome>::fail(ErrorCode::StateConflict,
                                           QStringLiteral("订单状态异常，请联系管理员"));
    }

    const auto pile = pileRepository_.findById(database, order.pileId);
    if (!pile.success) {
        return Result<SettleOutcome>::fail(pile.code, pile.message);
    }
    if (!pile.data.has_value()) {
        return Result<SettleOutcome>::fail(ErrorCode::NotFound,
                                           QStringLiteral("充电桩信息缺失"));
    }

    double finalKwh = 0.0;
    qint64 feeCent = 0;
    qint64 grossFeeCent = 0;
    qint64 elapsedSec = 0;
    if (order.status == QStringLiteral("charging")) {
        const QDateTime start = parseDbDateTime(order.startTime);
        const auto snapshot = computeChargeData(start, pile.data->powerKw,
                                                order.pricePerKwh, order.discountBps);
        if (!snapshot.success) {
            return Result<SettleOutcome>::fail(snapshot.code, snapshot.message);
        }
        finalKwh = snapshot.data.kwh;
        feeCent = snapshot.data.feeCent;
        grossFeeCent = snapshot.data.grossFeeCent;
        elapsedSec = snapshot.data.elapsedSec;
    } else {
        // Values were persisted when the order entered pending_settlement.
        finalKwh = order.chargeAmountKwh;
        feeCent = order.totalFeeCent;
        grossFeeCent = order.grossFeeCent;
        elapsedSec = qMax<qint64>(0, parseDbDateTime(order.startTime)
                               .secsTo(QDateTime::currentDateTime()));
    }

    const auto user = userRepository_.findById(database, order.userId);
    if (!user.success) {
        return Result<SettleOutcome>::fail(user.code, user.message);
    }
    if (!user.data.has_value()) {
        return Result<SettleOutcome>::fail(ErrorCode::NotFound,
                                           QStringLiteral("用户信息缺失"));
    }
    const qint64 balanceCent = user.data->balanceCent;

    SettleOutcome outcome;
    outcome.orderId = orderId;
    outcome.totalKwh = finalKwh;
    outcome.totalFeeCent = feeCent;
    outcome.grossFeeCent = grossFeeCent;
    outcome.membershipLevel = order.membershipLevel;
    outcome.discountBps = order.discountBps;

    if (balanceCent >= feeCent) {
        if (!database.transaction()) {
            return Result<SettleOutcome>::fail(ErrorCode::StorageError,
                QStringLiteral("cannot begin settle transaction"));
        }
        const auto balanceUpdate = userRepository_.updateBalance(
            database, order.userId, balanceCent - feeCent);
        const auto orderUpdate = orderRepository_.settleOrder(
            database, orderId, finalKwh, feeCent, grossFeeCent);
        const auto pileUpdate = pileRepository_.updateStatus(
            database, order.pileId, QStringLiteral("idle"), QStringLiteral("in_use"));
        const auto statsUpdate = pileRepository_.updateStats(
            database, order.pileId, 1, static_cast<double>(elapsedSec) / 3600.0);
        if (!balanceUpdate.success || !orderUpdate.success || !pileUpdate.success
            || !statsUpdate.success) {
            database.rollback();
            const Result<bool> firstFailure = !balanceUpdate.success ? balanceUpdate
                : (!orderUpdate.success ? orderUpdate
                : (!pileUpdate.success ? pileUpdate : statsUpdate));
            return Result<SettleOutcome>::fail(firstFailure.code, firstFailure.message);
        }
        if (!database.commit()) {
            return Result<SettleOutcome>::fail(ErrorCode::StorageError,
                QStringLiteral("cannot commit settle transaction"));
        }
        outcome.settled = true;
        outcome.balanceCent = balanceCent - feeCent;
        outcome.shortfallCent = 0;
        return Result<SettleOutcome>::ok(outcome);
    }

    // Insufficient balance: park in pending_settlement, keep the pile in use.
    if (!database.transaction()) {
        return Result<SettleOutcome>::fail(ErrorCode::StorageError,
            QStringLiteral("cannot begin pending-settlement transaction"));
    }
    std::optional<Result<bool>> failure;
    if (order.status == QStringLiteral("charging")) {
        const auto statusUpdate = orderRepository_.updateStatus(
            database, orderId, QStringLiteral("pending_settlement"));
        if (!statusUpdate.success) {
            failure = statusUpdate;
        }
    }
    if (!failure.has_value()) {
        const auto dataUpdate = orderRepository_.updateChargeData(
            database, orderId, finalKwh, feeCent, grossFeeCent);
        if (!dataUpdate.success) {
            failure = dataUpdate;
        }
    }
    if (failure.has_value()) {
        database.rollback();
        return Result<SettleOutcome>::fail(failure->code, failure->message);
    }
    if (!database.commit()) {
        return Result<SettleOutcome>::fail(ErrorCode::StorageError,
            QStringLiteral("cannot commit pending-settlement transaction"));
    }
    outcome.settled = false;
    outcome.balanceCent = balanceCent;
    outcome.shortfallCent = feeCent - balanceCent;
    return Result<SettleOutcome>::ok(outcome);
}

Result<bool> ChargeService::cancel(QSqlDatabase &database, qint64 userId,
                                   qint64 orderId) const
{
    const auto existing = orderRepository_.findById(database, orderId);
    if (!existing.success) {
        return Result<bool>::fail(existing.code, existing.message);
    }
    if (!existing.data.has_value()) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("订单不存在"));
    }
    if (existing.data->userId != userId) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("订单不存在"));
    }
    if (existing.data->status != QStringLiteral("reserved")) {
        return Result<bool>::fail(ErrorCode::StateConflict,
                                  QStringLiteral("仅已预约未开始的订单可以取消"));
    }

    if (!database.transaction()) {
        return Result<bool>::fail(ErrorCode::StorageError,
                                  QStringLiteral("cannot begin cancel transaction"));
    }
    const auto orderUpdate = orderRepository_.updateStatus(
        database, orderId, QStringLiteral("cancelled"));
    if (!orderUpdate.success) {
        database.rollback();
        return orderUpdate;
    }
    const auto pileUpdate = pileRepository_.updateStatus(
        database, existing.data->pileId, QStringLiteral("idle"),
        QStringLiteral("reserved"));
    if (!pileUpdate.success) {
        database.rollback();
        return Result<bool>::fail(pileUpdate.code, transitionFailureMessage(pileUpdate));
    }
    if (!database.commit()) {
        return Result<bool>::fail(ErrorCode::StorageError,
                                  QStringLiteral("cannot commit cancel transaction"));
    }
    return Result<bool>::ok(true);
}

} // namespace ev
