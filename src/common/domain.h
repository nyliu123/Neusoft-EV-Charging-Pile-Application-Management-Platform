#pragma once

#include <QString>

namespace evcs::domain {

enum class UserRole { User, Admin, Unknown };
enum class ChargerStatus { Idle, Reserved, Charging, Fault, Offline, Disabled, Unknown };
enum class ReservationStatus { Active, Used, Cancelled, Expired, Unknown };

QString toString(UserRole value);
QString toString(ChargerStatus value);
QString toString(ReservationStatus value);

UserRole userRoleFromString(const QString &value);
ChargerStatus chargerStatusFromString(const QString &value);
ReservationStatus reservationStatusFromString(const QString &value);

} // namespace evcs::domain
