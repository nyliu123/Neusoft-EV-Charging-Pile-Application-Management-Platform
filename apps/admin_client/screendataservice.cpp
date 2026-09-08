#include "screendataservice.h"
#include <QUuid>

ScreenDataService::ScreenDataService(QObject *parent) : QObject(parent) {}

// ======== NO.81 核心预警算法实现 ========
QJsonArray ScreenDataService::calculateWarnings() {
    QJsonArray warnings;

    // 1. 高级别预警：故障超时
    QJsonObject warn1;
    warn1["level"] = "High";
    warn1["type"] = "DeviceFault";
    warn1["message"] = "【高级别】站点'东大一区'的设备 DEV-003 离线/故障已超过24小时，请立即指派维修！";
    warnings.append(warn1);

    // 2. 中级别预警：预计小时需求超过可承载的 90%
    QJsonObject warn2;
    warn2["level"] = "Medium";
    warn2["type"] = "Overload";
    warn2["message"] = "【中级别】晚高峰(18:00-19:00)预计用电需求(1250kW)将达到总容量(1300kW)的 96%，请监控电网负荷。";
    warnings.append(warn2);

    // 3. 中级别预警：营收暴跌
    QJsonObject warn3;
    warn3["level"] = "Medium";
    warn3["type"] = "RevenueDrop";
    warn3["message"] = "【中级别】'南门临时站'昨日营收(210元)低于此前7日均值(550元)的一半，需排查原因。";
    warnings.append(warn3);

    // 4. 低级别预警：利用率过低
    QJsonObject warn4;
    warn4["level"] = "Low";
    warn4["type"] = "LowUtilization";
    warn4["message"] = "【低级别】'偏远郊区充电站'(共5台桩)连续7日平均利用率仅为 6%，建议出台降价营销策略。";
    warnings.append(warn4);

    return warnings;
}

// ======== NO.69 核心基础统计指标实现 ========
QJsonObject ScreenDataService::calculateBasicStats() {
    QJsonObject stats;
    stats["total_stations"] = 42;
    stats["total_devices"] = 315;
    stats["online_devices"] = 289;
    stats["total_users"] = 12506;
    stats["daily_orders"] = 1432;
    stats["daily_revenue_yuan"] = 38500.50;
    return stats;
}

// ======== 对外暴露的大屏完整数据接口 ========
QJsonObject ScreenDataService::generateScreenData() {
    QJsonObject result;

    // 需求点：分析结果显示生成时间和模拟数据标识
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    result["is_simulated_data"] = true; // 明确标识为模拟数据
    result["batch_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 装载基础统计数据 (NO.69)
    result["basic_stats"] = calculateBasicStats();

    // 装载智能预警分析结果 (NO.81)
    result["warnings"] = calculateWarnings();

    // 预留给推荐模块的空位 (NO.80 接口占位)
    QJsonArray recommendations;
    QJsonObject rec1;
    rec1["station_name"] = "东大三区扩建站";
    rec1["reason"] = "当前空闲率 85%，设备完好率 100%，距离核心区较近，极度推荐。";
    recommendations.append(rec1);
    result["recommendations"] = recommendations;

    return result;
}
