#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace ev {

struct AdminRecord {
    qint64 adminId = 0;
    QString username;
    QString createTime;
};

// UML-051: admins table data access.
// Only admin login uses this — no CRUD for admin accounts
// (they are pre-seeded or created by DBA).
class AdminRepository final {
public:
    // UML-034: verify credentials for login.
    // Matches both username AND password simultaneously — does not
    // distinguish "user not found" from "wrong password" to prevent
    // username enumeration.
    Result<std::optional<AdminRecord>> findByCredentials(
        QSqlDatabase &database,
        const QString &username,
        const QString &password) const;
};

} // namespace ev
