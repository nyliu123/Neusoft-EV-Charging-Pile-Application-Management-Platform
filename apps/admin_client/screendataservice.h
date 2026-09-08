#ifndef SCREENDATASERVICE_H
#define SCREENDATASERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

// 大屏数据与预警分析引擎 (完成需求 NO.69, NO.81)
class ScreenDataService : public QObject {
    Q_OBJECT
public:
    explicit ScreenDataService(QObject *parent = nullptr);

    // 获取大屏所需的全部指标 (含预警)
    QJsonObject generateScreenData();

private:
    // NO.81 核心预警算法
    QJsonArray calculateWarnings();

    // NO.69 核心基础统计指标
    QJsonObject calculateBasicStats();
};

#endif // SCREENDATASERVICE_H
