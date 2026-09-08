#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>

namespace ev {

class DatabaseManager final {
public:
    DatabaseManager(QString databasePath, int busyTimeoutMs = 5000);

    Result<QSqlDatabase> openForCurrentThread() const;
    Result<int> migrate(QSqlDatabase &database) const;

    // ── UML-053: transaction management ─────────────────────────────────
    // Auto-transaction wrapper: calls `fn`, commits on success,
    // rolls back on exception or false return.
    // Usage:
    //   auto result = db.executeTransaction([&]() {
    //       // multi-table writes here...
    //       return true;
    //   });
    Result<bool> executeTransaction(QSqlDatabase &database,
                                     const std::function<bool()> &fn) const;

private:
    QString connectionName() const;

    QString databasePath_;
    int busyTimeoutMs_;
};

} // namespace ev

