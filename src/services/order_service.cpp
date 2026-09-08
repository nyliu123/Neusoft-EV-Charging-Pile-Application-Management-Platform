#include "services/order_service.h"

#include "data/order_repository.h"
#include "services/user_service.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>

namespace ev {
namespace {

QDateTime orderTime(const QString &text)
{
    auto time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    return time;
}

QJsonValue nullableTime(const QString &text)
{
    return text.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(text);
}

} // namespace

Result<QJsonObject> OrderService::queryOrders(QSqlDatabase &database,
                                              qint64 authenticatedUserId) const
{
    if (authenticatedUserId <= 0) {
        return Result<QJsonObject>::fail(ErrorCode::Unauthorized,
                                         QStringLiteral("请先登录"));
    }
    const auto user = UserService().queryUserInfo(database, authenticatedUserId);
    if (!user.success) {
        return Result<QJsonObject>::fail(
            user.code == ErrorCode::NotFound ? ErrorCode::Unauthorized : user.code,
            user.message);
    }
    const auto records = OrderRepository().findByUser(database, authenticatedUserId);
    if (!records.success) {
        return Result<QJsonObject>::fail(records.code, QStringLiteral("订单读取失败，请稍后重试"));
    }
    static const QHash<QString, QString> statusTexts {
        {QStringLiteral("reserved"), QStringLiteral("预约中")},
        {QStringLiteral("charging"), QStringLiteral("充电中")},
        {QStringLiteral("pending_settlement"), QStringLiteral("待结算")},
        {QStringLiteral("settled"), QStringLiteral("已结算")},
        {QStringLiteral("cancelled"), QStringLiteral("已取消")}
    };
    QJsonArray orders;
    for (const auto &record : records.data) {
        QJsonValue duration(QJsonValue::Null);
        const auto start = orderTime(record.startTime);
        const auto end = orderTime(record.endTime);
        if (start.isValid() && end.isValid() && end >= start) {
            duration = start.msecsTo(end) / 3600000.0;
        }
        orders.append(QJsonObject {
            {QStringLiteral("order_id"), record.orderId},
            {QStringLiteral("station_name"), record.stationName},
            {QStringLiteral("pile_number"), record.pileNumber},
            {QStringLiteral("status"), record.status},
            {QStringLiteral("status_text"), statusTexts.value(record.status, QStringLiteral("未知状态"))},
            {QStringLiteral("reserve_time"), record.reserveTime},
            {QStringLiteral("start_time"), nullableTime(record.startTime)},
            {QStringLiteral("end_time"), nullableTime(record.endTime)},
            {QStringLiteral("duration_hours"), duration},
            {QStringLiteral("charge_amount_kwh"), record.chargeAmountKwh},
            {QStringLiteral("price_per_kwh"), record.pricePerKwh},
            {QStringLiteral("total_fee"), record.totalFeeCent / 100.0}
        });
    }
    return Result<QJsonObject>::ok({{QStringLiteral("orders"), orders}});
}

} // namespace ev
