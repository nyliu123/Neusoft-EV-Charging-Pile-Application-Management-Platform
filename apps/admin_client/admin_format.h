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
        return {0x55, 0x78, 0x3e};
    }
    if (status == QStringLiteral("reserved")) {
        return {0xb5, 0x89, 0x38};
    }
    if (status == QStringLiteral("in_use")) {
        return {0x3e, 0x79, 0x70};
    }
    if (status == QStringLiteral("fault")) {
        return {0xb2, 0x49, 0x3c};
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

inline QColor userStatusColor(const QString &status)
{
    if (status == QStringLiteral("normal")) {
        return {0x55, 0x78, 0x3e};
    }
    if (status == QStringLiteral("frozen")) {
        return {0xb2, 0x49, 0x3c};
    }
    return {0x75, 0x75, 0x75};
}

// "13800138000" -> "138****8000"
inline QString maskPhone(const QString &phone)
{
    if (phone.size() == 11) {
        return phone.left(3) + QStringLiteral("****") + phone.right(4);
    }
    return phone;
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
        return QStringLiteral("已结算");
    }
    if (status == QStringLiteral("cancelled")) {
        return QStringLiteral("已取消");
    }
    return status;
}

inline QColor orderStatusColor(const QString &status)
{
    if (status == QStringLiteral("reserved")) {
        return {0xb5, 0x89, 0x38};
    }
    if (status == QStringLiteral("charging")) {
        return {0x3e, 0x79, 0x70};
    }
    if (status == QStringLiteral("pending_settlement")) {
        return {0xef, 0x6c, 0x00};
    }
    if (status == QStringLiteral("settled")) {
        return {0x55, 0x78, 0x3e};
    }
    if (status == QStringLiteral("cancelled")) {
        return {0x9e, 0x9e, 0x9e};
    }
    return {0x75, 0x75, 0x75};
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
