#include "domain.h"

namespace evcs::domain {

QString toString(UserRole value)
{
    switch (value) {
    case UserRole::User: return QStringLiteral("user");
    case UserRole::Admin: return QStringLiteral("admin");
    case UserRole::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString toString(ChargerStatus value)
{
    switch (value) {
    case ChargerStatus::Idle: return QStringLiteral("idle");
    case ChargerStatus::Reserved: return QStringLiteral("reserved");
    case ChargerStatus::Charging: return QStringLiteral("charging");
    case ChargerStatus::Fault: return QStringLiteral("fault");
    case ChargerStatus::Offline: return QStringLiteral("offline");
    case ChargerStatus::Disabled: return QStringLiteral("disabled");
    case ChargerStatus::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString toString(ReservationStatus value)
{
    switch (value) {
    case ReservationStatus::Active: return QStringLiteral("active");
    case ReservationStatus::Used: return QStringLiteral("used");
    case ReservationStatus::Cancelled: return QStringLiteral("cancelled");
    case ReservationStatus::Expired: return QStringLiteral("expired");
    case ReservationStatus::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

UserRole userRoleFromString(const QString &value)
{
    if (value == QStringLiteral("user")) return UserRole::User;
    if (value == QStringLiteral("admin")) return UserRole::Admin;
    return UserRole::Unknown;
}

ChargerStatus chargerStatusFromString(const QString &value)
{
    if (value == QStringLiteral("idle")) return ChargerStatus::Idle;
    if (value == QStringLiteral("reserved")) return ChargerStatus::Reserved;
    if (value == QStringLiteral("charging")) return ChargerStatus::Charging;
    if (value == QStringLiteral("fault")) return ChargerStatus::Fault;
    if (value == QStringLiteral("offline")) return ChargerStatus::Offline;
    if (value == QStringLiteral("disabled")) return ChargerStatus::Disabled;
    return ChargerStatus::Unknown;
}

ReservationStatus reservationStatusFromString(const QString &value)
{
    if (value == QStringLiteral("active")) return ReservationStatus::Active;
    if (value == QStringLiteral("used")) return ReservationStatus::Used;
    if (value == QStringLiteral("cancelled")) return ReservationStatus::Cancelled;
    if (value == QStringLiteral("expired")) return ReservationStatus::Expired;
    return ReservationStatus::Unknown;
}

} // namespace evcs::domain
