#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>

namespace ev {

struct AdminInfo {
    int adminId = 0;
    QString username;
};

class AdminAuthService final {
public:
    // Authenticate an admin by username and password.
    // Returns admin info on success, or appropriate error code on failure.
    static Result<AdminInfo> authenticate(const QString &username,
                                          const QString &password,
                                          QSqlDatabase &database);
};

} // namespace ev
