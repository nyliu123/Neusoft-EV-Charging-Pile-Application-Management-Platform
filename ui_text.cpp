#include "ui_text.h"

#include <QHash>

QString statusText(const QString &value)
{
    static const QHash<QString, QString> texts{
        {QStringLiteral("active"), QStringLiteral("正常")},
        {QStringLiteral("disabled"), QStringLiteral("停用")},
        {QStringLiteral("idle"), QStringLiteral("空闲")},
        {QStringLiteral("reserved"), QStringLiteral("已预约")},
        {QStringLiteral("charging"), QStringLiteral("充电中")},
        {QStringLiteral("fault"), QStringLiteral("故障")},
        {QStringLiteral("offline"), QStringLiteral("离线")},
        {QStringLiteral("used"), QStringLiteral("已使用")},
        {QStringLiteral("cancelled"), QStringLiteral("已取消")},
        {QStringLiteral("expired"), QStringLiteral("已过期")},
        {QStringLiteral("finished"), QStringLiteral("已完成")},
        {QStringLiteral("interrupted"), QStringLiteral("已中断")},
        {QStringLiteral("paid"), QStringLiteral("已结算")},
        {QStringLiteral("pending"), QStringLiteral("待结算")},
        {QStringLiteral("settled"), QStringLiteral("已结算")},
        {QStringLiteral("pending_settlement"), QStringLiteral("待结算")},
        {QStringLiteral("open"), QStringLiteral("待处理")},
        {QStringLiteral("processing"), QStringLiteral("处理中")},
        {QStringLiteral("resolved"), QStringLiteral("已解决")}
    };
    return texts.value(value, value);
}

QString roleText(const QString &value)
{
    if (value == QStringLiteral("admin")) return QStringLiteral("管理员");
    if (value == QStringLiteral("user")) return QStringLiteral("普通用户");
    return value;
}
