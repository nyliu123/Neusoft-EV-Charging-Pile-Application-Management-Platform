#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>

namespace ev {

// Seeds the database with a default admin account if none exists.
// This ensures the system is accessible on first run.
// Default credentials: admin / admin123
class AdminSeeder final {
public:
    // Check if any admin exists. If not, create the default one.
    // Returns the number of admins created (0 or 1).
    static Result<int> seedIfNeeded(QSqlDatabase &database);

    static constexpr const char *kDefaultUsername = "admin";
    static constexpr const char *kDefaultPassword = "admin123";
};

} // namespace ev
