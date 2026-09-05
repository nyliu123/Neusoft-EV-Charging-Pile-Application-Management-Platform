#pragma once

#include <QColor>
#include <QLocale>
#include <QString>

namespace ev {

inline QString pileStatusText(const QString &status)
{
    if (status == QStringLiteral("idle")) {
        return QStringLiteral("空闲");
    }
    if (status == QStringLiteral("reserved")) {
        return QStringLiteral("已预约");
    }
    if (status == QStringLiteral("in_use")) {
        return QStringLiteral("使用中");
    }
    if (status == QStringLiteral("fault")) {
        return QStringLiteral("故障");
    }
    return status;
}

inline QColor pileStatusColor(const QString &status)
{
    if (status == QStringLiteral("idle")) {
        return {0x2e, 0x7d, 0x32};
    }
    if (status == QStringLiteral("reserved")) {
        return {0xf9, 0xa8, 0x25};
    }
    if (status == QStringLiteral("in_use")) {
        return {0x15, 0x65, 0xc0};
    }
    if (status == QStringLiteral("fault")) {
        return {0xc6, 0x28, 0x28};
    }
    return {0x75, 0x75, 0x75};
}

inline QString pileTypeText(const QString &type)
{
    if (type == QStringLiteral("fast")) {
        return QStringLiteral("快充");
    }
    if (type == QStringLiteral("slow")) {
        return QStringLiteral("慢充");
    }
    return type;
}

inline QString userStatusText(const QString &status)
{
    if (status == QStringLiteral("normal")) {
        return QStringLiteral("正常");
    }
    if (status == QStringLiteral("frozen")) {
        return QStringLiteral("已冻结");
    }
    return status;
}

inline QString orderStatusText(const QString &status)
{
    if (status == QStringLiteral("reserved")) {
        return QStringLiteral("已预约");
    }
    if (status == QStringLiteral("charging")) {
        return QStringLiteral("充电中");
    }
    if (status == QStringLiteral("pending_settlement")) {
        return QStringLiteral("待结算");
    }
    if (status == QStringLiteral("settled")) {
        return QStringLiteral("已完成");
    }
    if (status == QStringLiteral("cancelled")) {
        return QStringLiteral("已取消");
    }
    return status;
}

inline QString formatAmount(double value)
{
    return QLocale(QLocale::Chinese).toString(value, 'f', 2);
}

// "2026-09-05 14:30:00" -> "2026-09-05 14:30"
inline QString formatDateTimeShort(const QString &databaseText)
{
    return databaseText.left(16);
}

} // namespace ev
