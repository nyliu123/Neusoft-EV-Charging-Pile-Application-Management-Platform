#include "admin_handler.h"

#include "services/session_manager.h"

#include <QDate>
#include <QDebug>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantList>
#include <utility>

namespace ev {

AdminHandler::AdminHandler(QSqlDatabase database, SessionManager &sessionManager)
    : database_(std::move(database)), sessionManager_(sessionManager)
{
}

QJsonObject AdminHandler::processQuery(const QString &type, const QJsonObject &params)
{
    if (type == QStringLiteral("dashboard_overview")) {
        return queryDashboardOverview(params);
    }
    if (type == QStringLiteral("dashboard_summary")) {
        return queryDashboardSummary();
    }
    if (type == QStringLiteral("revenue_trend")) {
        return queryRevenueTrend(params);
    }
    if (type == QStringLiteral("pile_status_stats")) {
        return queryPileStatusStats();
    }
    if (type == QStringLiteral("pile_list")) {
        return queryPileList(params);
    }
    if (type == QStringLiteral("station_list")) {
        return queryStationList();
    }
    if (type == QStringLiteral("station_detail")) {
        return queryStationDetail(params);
    }
    if (type == QStringLiteral("user_list")) {
        return queryUserList(params);
    }
    if (type == QStringLiteral("order_list")) {
        return queryOrderList(params);
    }
    return failBody(type, ErrorCode::InvalidInput,
                    QStringLiteral("未知的查询类型：%1").arg(type));
}

QJsonObject AdminHandler::queryDashboardOverview(const QJsonObject &params)
{
    const QJsonObject summary = queryDashboardSummary();
    const QJsonObject trend = queryRevenueTrend(params);
    const QJsonObject stats = queryPileStatusStats();
    for (const QJsonObject &part : {summary, trend, stats}) {
        if (!part.value(QStringLiteral("success")).toBool()) {
            return failBody(QStringLiteral("dashboard_overview"), ErrorCode::StorageError,
                part.value(QStringLiteral("message")).toString());
        }
    }

    const auto resultOf = [](const QJsonObject &body) {
        return body.value(QStringLiteral("data")).toObject()
            .value(QStringLiteral("result")).toObject();
    };
    return okBody(QStringLiteral("dashboard_overview"), QJsonObject {
        {QStringLiteral("summary"), resultOf(summary)},
        {QStringLiteral("trend"), resultOf(trend)},
        {QStringLiteral("stats"), resultOf(stats)}
    });
}

QJsonObject AdminHandler::processAction(const QString &type, const QJsonObject &params)
{
    if (type == QStringLiteral("restart_pile")) {
        return actionRestartPile(params);
    }
    if (type == QStringLiteral("add_station")) {
        return actionAddStation(params);
    }
    if (type == QStringLiteral("set_user_status")) {
        return actionSetUserStatus(params);
    }
    return failBody(type, ErrorCode::InvalidInput,
                    QStringLiteral("未知的操作类型：%1").arg(type));
}

QJsonObject AdminHandler::queryDashboardSummary()
{
    QSqlQuery settledQuery(database_);
    settledQuery.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(charge_amount_kwh), 0), COALESCE(SUM(total_fee), 0) "
        "FROM orders WHERE status = 'settled'"));
    if (!settledQuery.exec()) {
        qWarning().noquote() << "admin handler: dashboard summary failed:"
                             << settledQuery.lastError().text();
        return failBody(QStringLiteral("dashboard_summary"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    double totalKwh = 0.0;
    double totalRevenue = 0.0;
    if (settledQuery.next()) {
        totalKwh = settledQuery.value(0).toDouble();
        totalRevenue = settledQuery.value(1).toDouble();
    }

    int totalOrders = 0;
    QSqlQuery orderQuery(database_);
    if (!orderQuery.exec(QStringLiteral("SELECT COUNT(*) FROM orders"))) {
        return failBody(QStringLiteral("dashboard_summary"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    if (orderQuery.next()) {
        totalOrders = orderQuery.value(0).toInt();
    }

    int totalUsers = 0;
    QSqlQuery userQuery(database_);
    if (!userQuery.exec(QStringLiteral("SELECT COUNT(*) FROM users"))) {
        return failBody(QStringLiteral("dashboard_summary"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    if (userQuery.next()) {
        totalUsers = userQuery.value(0).toInt();
    }

    return okBody(QStringLiteral("dashboard_summary"), QJsonObject {
        {QStringLiteral("total_kwh"), totalKwh},
        {QStringLiteral("total_revenue"), totalRevenue},
        {QStringLiteral("total_users"), totalUsers},
        {QStringLiteral("total_orders"), totalOrders}
    });
}

QJsonObject AdminHandler::queryRevenueTrend(const QJsonObject &params)
{
    int days = 7;
    const QJsonValue daysValue = params.value(QStringLiteral("days"));
    if (daysValue.isDouble()) {
        const int requested = daysValue.toInt();
        if (requested >= 1 && requested <= 90) {
            days = requested;
        }
    }

    QHash<QString, double> revenueByDate;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT DATE(end_time), COALESCE(SUM(total_fee), 0) FROM orders "
        "WHERE status = 'settled' AND end_time IS NOT NULL "
        "AND end_time >= datetime('now', 'localtime', ?, 'start of day') "
        "GROUP BY DATE(end_time)"));
    query.addBindValue(QStringLiteral("-%1 days").arg(days - 1));
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: revenue trend failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("revenue_trend"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    while (query.next()) {
        revenueByDate.insert(query.value(0).toString(), query.value(1).toDouble());
    }

    // Zero-fill every day so the chart always shows a continuous range.
    const QDate today = QDate::currentDate();
    QJsonArray points;
    for (int offset = days - 1; offset >= 0; --offset) {
        const QString date = today.addDays(-offset).toString(QStringLiteral("yyyy-MM-dd"));
        points.append(QJsonObject {
            {QStringLiteral("date"), date},
            {QStringLiteral("revenue"), revenueByDate.value(date, 0.0)}
        });
    }

    return okBody(QStringLiteral("revenue_trend"), QJsonObject {
        {QStringLiteral("days"), days},
        {QStringLiteral("points"), points}
    });
}

QJsonObject AdminHandler::queryPileStatusStats()
{
    QHash<QString, int> counts;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT status, COUNT(*) FROM charging_piles GROUP BY status"))) {
        qWarning().noquote() << "admin handler: pile stats failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("pile_status_stats"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    while (query.next()) {
        counts.insert(query.value(0).toString(), query.value(1).toInt());
    }

    const QHash<QString, QString> knownStatuses {
        {QStringLiteral("idle"), QStringLiteral("空闲")},
        {QStringLiteral("reserved"), QStringLiteral("已预约")},
        {QStringLiteral("in_use"), QStringLiteral("使用中")},
        {QStringLiteral("fault"), QStringLiteral("故障")}
    };
    QJsonArray stats;
    for (auto it = knownStatuses.constBegin(); it != knownStatuses.constEnd(); ++it) {
        stats.append(QJsonObject {
            {QStringLiteral("status"), it.key()},
            {QStringLiteral("label"), it.value()},
            {QStringLiteral("count"), counts.value(it.key(), 0)}
        });
    }
    return okBody(QStringLiteral("pile_status_stats"),
                  QJsonObject {{QStringLiteral("stats"), stats}});
}

QJsonObject AdminHandler::queryPileList(const QJsonObject &params)
{
    QStringList conditions;
    QVariantList bindings;

    const QJsonValue stationId = params.value(QStringLiteral("station_id"));
    if (stationId.isDouble() && stationId.toInt() > 0) {
        conditions << QStringLiteral("p.station_id = ?");
        bindings << stationId.toInt();
    }
    const QJsonValue status = params.value(QStringLiteral("status"));
    if (status.isString() && !status.toString().isEmpty()) {
        conditions << QStringLiteral("p.status = ?");
        bindings << status.toString();
    }

    QString sql = QStringLiteral(
        "SELECT p.pile_id, p.pile_number, p.pile_type, p.power_kw, p.status, "
        "p.total_charge_count, p.total_charge_duration, "
        "s.station_id, COALESCE(s.station_name, '') "
        "FROM charging_piles p "
        "LEFT JOIN charging_stations s ON p.station_id = s.station_id");
    if (!conditions.isEmpty()) {
        sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    }
    sql += QStringLiteral(" ORDER BY s.station_name ASC, p.pile_number ASC");

    QSqlQuery query(database_);
    query.prepare(sql);
    for (const QVariant &binding : bindings) {
        query.addBindValue(binding);
    }
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: pile list failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("pile_list"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }

    QJsonArray piles;
    while (query.next()) {
        piles.append(QJsonObject {
            {QStringLiteral("pile_id"), query.value(0).toInt()},
            {QStringLiteral("pile_number"), query.value(1).toString()},
            {QStringLiteral("pile_type"), query.value(2).toString()},
            {QStringLiteral("power_kw"), query.value(3).toDouble()},
            {QStringLiteral("status"), query.value(4).toString()},
            {QStringLiteral("total_charge_count"), query.value(5).toInt()},
            {QStringLiteral("total_charge_duration"), query.value(6).toDouble()},
            {QStringLiteral("station_id"), query.value(7).toInt()},
            {QStringLiteral("station_name"), query.value(8).toString()}
        });
    }
    return okBody(QStringLiteral("pile_list"),
                  QJsonObject {{QStringLiteral("piles"), piles}});
}

QJsonObject AdminHandler::queryStationList()
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT s.station_id, s.station_name, COALESCE(s.address, ''), "
            "COALESCE(s.longitude, 0), COALESCE(s.latitude, 0), s.price_per_kwh, "
            "COUNT(p.pile_id) "
            "FROM charging_stations s "
            "LEFT JOIN charging_piles p ON s.station_id = p.station_id "
            "GROUP BY s.station_id ORDER BY s.station_id ASC"))) {
        qWarning().noquote() << "admin handler: station list failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("station_list"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }

    QJsonArray stations;
    while (query.next()) {
        stations.append(QJsonObject {
            {QStringLiteral("station_id"), query.value(0).toInt()},
            {QStringLiteral("station_name"), query.value(1).toString()},
            {QStringLiteral("address"), query.value(2).toString()},
            {QStringLiteral("longitude"), query.value(3).toDouble()},
            {QStringLiteral("latitude"), query.value(4).toDouble()},
            {QStringLiteral("price_per_kwh"), query.value(5).toDouble()},
            {QStringLiteral("pile_count"), query.value(6).toInt()}
        });
    }
    return okBody(QStringLiteral("station_list"),
                  QJsonObject {{QStringLiteral("stations"), stations}});
}

QJsonObject AdminHandler::queryStationDetail(const QJsonObject &params)
{
    const qint64 stationId = params.value(QStringLiteral("station_id")).toInteger();
    if (stationId <= 0) {
        return failBody(QStringLiteral("station_detail"), ErrorCode::InvalidInput,
                        QStringLiteral("参数错误：缺少站点编号"));
    }

    QSqlQuery stationQuery(database_);
    stationQuery.prepare(QStringLiteral(
        "SELECT station_name, COALESCE(address, ''), COALESCE(longitude, 0), "
        "COALESCE(latitude, 0), price_per_kwh "
        "FROM charging_stations WHERE station_id = ?"));
    stationQuery.addBindValue(stationId);
    if (!stationQuery.exec()) {
        qWarning().noquote() << "admin handler: station detail failed:"
                             << stationQuery.lastError().text();
        return failBody(QStringLiteral("station_detail"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    if (!stationQuery.next()) {
        return failBody(QStringLiteral("station_detail"), ErrorCode::NotFound,
                        QStringLiteral("站点不存在"));
    }

    QSqlQuery pileQuery(database_);
    pileQuery.prepare(QStringLiteral(
        "SELECT pile_id, pile_number, pile_type, power_kw, status, "
        "total_charge_count, total_charge_duration "
        "FROM charging_piles WHERE station_id = ? ORDER BY pile_number ASC"));
    pileQuery.addBindValue(stationId);
    if (!pileQuery.exec()) {
        qWarning().noquote() << "admin handler: station detail piles failed:"
                             << pileQuery.lastError().text();
        return failBody(QStringLiteral("station_detail"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }

    QJsonArray piles;
    while (pileQuery.next()) {
        piles.append(QJsonObject {
            {QStringLiteral("pile_id"), pileQuery.value(0).toInt()},
            {QStringLiteral("pile_number"), pileQuery.value(1).toString()},
            {QStringLiteral("pile_type"), pileQuery.value(2).toString()},
            {QStringLiteral("power_kw"), pileQuery.value(3).toDouble()},
            {QStringLiteral("status"), pileQuery.value(4).toString()},
            {QStringLiteral("total_charge_count"), pileQuery.value(5).toInt()},
            {QStringLiteral("total_charge_duration"), pileQuery.value(6).toDouble()}
        });
    }

    return okBody(QStringLiteral("station_detail"), QJsonObject {
        {QStringLiteral("station"), QJsonObject {
            {QStringLiteral("station_id"), stationId},
            {QStringLiteral("station_name"), stationQuery.value(0).toString()},
            {QStringLiteral("address"), stationQuery.value(1).toString()},
            {QStringLiteral("longitude"), stationQuery.value(2).toDouble()},
            {QStringLiteral("latitude"), stationQuery.value(3).toDouble()},
            {QStringLiteral("price_per_kwh"), stationQuery.value(4).toDouble()}
        }},
        {QStringLiteral("piles"), piles}
    });
}

QJsonObject AdminHandler::queryUserList(const QJsonObject &params)
{
    const QString keyword = params.value(QStringLiteral("keyword")).toString().trimmed();

    QString sql = QStringLiteral(
        "SELECT user_id, phone, COALESCE(nickname, ''), COALESCE(balance, 0), "
        "register_time, status FROM users");
    const bool hasKeyword = !keyword.isEmpty();
    if (hasKeyword) {
        sql += QStringLiteral(" WHERE phone LIKE ? ESCAPE '\\'");
    }
    sql += QStringLiteral(" ORDER BY register_time DESC");

    QSqlQuery query(database_);
    query.prepare(sql);
    if (hasKeyword) {
        QString escaped = keyword;
        escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
        escaped.replace(QStringLiteral("%"), QStringLiteral("\\%"));
        escaped.replace(QStringLiteral("_"), QStringLiteral("\\_"));
        query.addBindValue(QStringLiteral("%1%2%3")
                               .arg(QStringLiteral("%"), escaped, QStringLiteral("%")));
    }
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: user list failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("user_list"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }

    QJsonArray users;
    while (query.next()) {
        users.append(QJsonObject {
            {QStringLiteral("user_id"), query.value(0).toInt()},
            {QStringLiteral("phone"), query.value(1).toString()},
            {QStringLiteral("nickname"), query.value(2).toString()},
            {QStringLiteral("balance"), query.value(3).toDouble()},
            {QStringLiteral("register_time"), query.value(4).toString()},
            {QStringLiteral("status"), query.value(5).toString()}
        });
    }
    return okBody(QStringLiteral("user_list"),
                  QJsonObject {{QStringLiteral("users"), users}});
}

QJsonObject AdminHandler::queryOrderList(const QJsonObject &params)
{
    QString firstReserveDate;
    QString lastReserveDate;
    QSqlQuery rangeQuery(database_);
    if (!rangeQuery.exec(QStringLiteral(
            "SELECT MIN(DATE(reserve_time)), MAX(DATE(reserve_time)) "
            "FROM orders WHERE reserve_time IS NOT NULL"))) {
        qWarning().noquote() << "admin handler: order date range failed:"
                             << rangeQuery.lastError().text();
        return failBody(QStringLiteral("order_list"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }
    if (rangeQuery.next()) {
        firstReserveDate = rangeQuery.value(0).toString();
        lastReserveDate = rangeQuery.value(1).toString();
    }

    QStringList conditions;
    QVariantList bindings;

    const QJsonValue status = params.value(QStringLiteral("status"));
    if (status.isString() && !status.toString().isEmpty()) {
        conditions << QStringLiteral("o.status = ?");
        bindings << status.toString();
    }
    const QJsonValue stationId = params.value(QStringLiteral("station_id"));
    if (stationId.isDouble() && stationId.toInt() > 0) {
        conditions << QStringLiteral("o.station_id = ?");
        bindings << stationId.toInt();
    }
    const QJsonValue startDate = params.value(QStringLiteral("start_date"));
    if (startDate.isString() && !startDate.toString().isEmpty()) {
        conditions << QStringLiteral("o.reserve_time >= ?");
        bindings << startDate.toString() + QStringLiteral(" 00:00:00");
    }
    const QJsonValue endDate = params.value(QStringLiteral("end_date"));
    if (endDate.isString() && !endDate.toString().isEmpty()) {
        conditions << QStringLiteral("o.reserve_time <= ?");
        bindings << endDate.toString() + QStringLiteral(" 23:59:59");
    }

    QString sql = QStringLiteral(
        "SELECT o.order_id, o.status, o.reserve_time, "
        "COALESCE(o.start_time, ''), COALESCE(o.end_time, ''), "
        "o.charge_amount_kwh, o.price_per_kwh, o.total_fee, "
        "COALESCE(u.phone, ''), COALESCE(u.nickname, ''), "
        "COALESCE(p.pile_number, ''), COALESCE(p.pile_type, ''), "
        "COALESCE(s.station_name, '') "
        "FROM orders o "
        "LEFT JOIN users u ON o.user_id = u.user_id "
        "LEFT JOIN charging_piles p ON o.pile_id = p.pile_id "
        "LEFT JOIN charging_stations s ON o.station_id = s.station_id");
    if (!conditions.isEmpty()) {
        sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    }
    sql += QStringLiteral(" ORDER BY o.reserve_time DESC");

    QSqlQuery query(database_);
    query.prepare(sql);
    for (const QVariant &binding : bindings) {
        query.addBindValue(binding);
    }
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: order list failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("order_list"), ErrorCode::StorageError,
                        QStringLiteral("数据查询失败，请稍后重试"));
    }

    QJsonArray orders;
    while (query.next()) {
        orders.append(QJsonObject {
            {QStringLiteral("order_id"), query.value(0).toInt()},
            {QStringLiteral("status"), query.value(1).toString()},
            {QStringLiteral("reserve_time"), query.value(2).toString()},
            {QStringLiteral("start_time"), query.value(3).toString()},
            {QStringLiteral("end_time"), query.value(4).toString()},
            {QStringLiteral("charge_amount_kwh"), query.value(5).toDouble()},
            {QStringLiteral("price_per_kwh"), query.value(6).toDouble()},
            {QStringLiteral("total_fee"), query.value(7).toDouble()},
            {QStringLiteral("user_phone"), query.value(8).toString()},
            {QStringLiteral("user_nickname"), query.value(9).toString()},
            {QStringLiteral("pile_number"), query.value(10).toString()},
            {QStringLiteral("pile_type"), query.value(11).toString()},
            {QStringLiteral("station_name"), query.value(12).toString()}
        });
    }
    return okBody(QStringLiteral("order_list"), QJsonObject {
        {QStringLiteral("orders"), orders},
        {QStringLiteral("first_reserve_date"), firstReserveDate},
        {QStringLiteral("last_reserve_date"), lastReserveDate}
    });
}

QJsonObject AdminHandler::actionRestartPile(const QJsonObject &params)
{
    const qint64 pileId = params.value(QStringLiteral("pile_id")).toInteger();
    if (pileId <= 0) {
        return failBody(QStringLiteral("restart_pile"), ErrorCode::InvalidInput,
                        QStringLiteral("参数错误：缺少充电桩编号"));
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE charging_piles SET status = 'idle' "
        "WHERE pile_id = ? AND status = 'fault'"));
    query.addBindValue(pileId);
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: restart pile failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("restart_pile"), ErrorCode::StorageError,
                        QStringLiteral("操作失败，请稍后重试"));
    }
    if (query.numRowsAffected() != 1) {
        return failBody(QStringLiteral("restart_pile"), ErrorCode::StateConflict,
                        QStringLiteral("该充电桩当前状态无需重启"));
    }
    qInfo().noquote() << "admin action: restart pile" << pileId;
    return okBody(QStringLiteral("restart_pile"), QJsonObject {},
                  QStringLiteral("充电桩已重启成功，状态恢复为空闲"));
}

QJsonObject AdminHandler::actionAddStation(const QJsonObject &params)
{
    const QString name = params.value(QStringLiteral("station_name")).toString().trimmed();
    const QString address = params.value(QStringLiteral("address")).toString().trimmed();
    const QJsonValue longitudeValue = params.value(QStringLiteral("longitude"));
    const QJsonValue latitudeValue = params.value(QStringLiteral("latitude"));
    const QJsonValue priceValue = params.value(QStringLiteral("price_per_kwh"));

    if (name.isEmpty() || name.size() > 100) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("站点名称必填且不超过 100 字"));
    }
    if (address.isEmpty() || address.size() > 255) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("站点地址必填且不超过 255 字"));
    }
    if (!longitudeValue.isDouble() || !latitudeValue.isDouble() || !priceValue.isDouble()) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("请填写完整的经度、纬度和电价"));
    }
    const double longitude = longitudeValue.toDouble();
    const double latitude = latitudeValue.toDouble();
    const double price = priceValue.toDouble();
    if (longitude < -180.0 || longitude > 180.0) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("经度范围应在 -180 到 180 之间"));
    }
    if (latitude < -90.0 || latitude > 90.0) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("纬度范围应在 -90 到 90 之间"));
    }
    if (price <= 0.0 || price >= 1000.0) {
        return failBody(QStringLiteral("add_station"), ErrorCode::InvalidInput,
                        QStringLiteral("电价应大于 0 且小于 1000"));
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_stations "
        "(station_name, address, longitude, latitude, price_per_kwh) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(address);
    query.addBindValue(longitude);
    query.addBindValue(latitude);
    query.addBindValue(price);
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: add station failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("add_station"), ErrorCode::StorageError,
                        QStringLiteral("创建失败，请稍后重试"));
    }

    const int stationId = query.lastInsertId().toInt();
    qInfo().noquote() << "admin action: add station" << name << "id" << stationId;
    return okBody(QStringLiteral("add_station"),
                  QJsonObject {{QStringLiteral("station_id"), stationId}},
                  QStringLiteral("充电站创建成功"));
}

QJsonObject AdminHandler::actionSetUserStatus(const QJsonObject &params)
{
    const qint64 userId = params.value(QStringLiteral("user_id")).toInteger();
    const QString targetStatus = params.value(QStringLiteral("status")).toString();
    if (userId <= 0
        || (targetStatus != QStringLiteral("frozen")
            && targetStatus != QStringLiteral("normal"))) {
        return failBody(QStringLiteral("set_user_status"), ErrorCode::InvalidInput,
                        QStringLiteral("参数错误"));
    }

    QSqlQuery query(database_);
    if (targetStatus == QStringLiteral("frozen")) {
        query.prepare(QStringLiteral(
            "UPDATE users SET status = 'frozen' WHERE user_id = ? AND status = 'normal'"));
        query.addBindValue(userId);
        if (!query.exec()) {
            qWarning().noquote() << "admin handler: freeze user failed:"
                                 << query.lastError().text();
            return failBody(QStringLiteral("set_user_status"), ErrorCode::StorageError,
                            QStringLiteral("操作失败，请稍后重试"));
        }
        if (query.numRowsAffected() != 1) {
            return failBody(QStringLiteral("set_user_status"), ErrorCode::StateConflict,
                            QStringLiteral("该用户当前状态无法执行冻结操作"));
        }
        // Freezing a user immediately kills every live session.
        sessionManager_.removeByUserId(userId);
        qInfo().noquote() << "admin action: freeze user" << userId;
        return okBody(QStringLiteral("set_user_status"), QJsonObject {},
                      QStringLiteral("用户已冻结，其在线会话已被强制下线"));
    }

    query.prepare(QStringLiteral(
        "UPDATE users SET status = 'normal' WHERE user_id = ? AND status = 'frozen'"));
    query.addBindValue(userId);
    if (!query.exec()) {
        qWarning().noquote() << "admin handler: unfreeze user failed:"
                             << query.lastError().text();
        return failBody(QStringLiteral("set_user_status"), ErrorCode::StorageError,
                        QStringLiteral("操作失败，请稍后重试"));
    }
    if (query.numRowsAffected() != 1) {
        return failBody(QStringLiteral("set_user_status"), ErrorCode::StateConflict,
                        QStringLiteral("该用户当前状态无需解冻"));
    }
    qInfo().noquote() << "admin action: unfreeze user" << userId;
    return okBody(QStringLiteral("set_user_status"), QJsonObject {},
                  QStringLiteral("用户已解冻"));
}

QJsonObject AdminHandler::okBody(const QString &type, const QJsonObject &result,
                                 const QString &message)
{
    return QJsonObject {
        {QStringLiteral("success"), true},
        {QStringLiteral("code"), QStringLiteral("OK")},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), QJsonObject {
            {QStringLiteral("type"), type},
            {QStringLiteral("result"), result}
        }}
    };
}

QJsonObject AdminHandler::failBody(const QString &type, ErrorCode code,
                                   const QString &message)
{
    return QJsonObject {
        {QStringLiteral("success"), false},
        {QStringLiteral("code"), errorCodeName(code)},
        {QStringLiteral("message"), message},
        {QStringLiteral("data"), QJsonObject {
            {QStringLiteral("type"), type},
            {QStringLiteral("result"), QJsonObject {}}
        }}
    };
}

} // namespace ev
