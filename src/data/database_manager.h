#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>

namespace ev {

class DatabaseManager final {
public:
    DatabaseManager(QString databasePath, int busyTimeoutMs = 5000);

    Result<QSqlDatabase> openForCurrentThread() const;
    Result<int> migrate(QSqlDatabase &database) const;

private:
    QString connectionName() const;

    QString databasePath_;
    int busyTimeoutMs_;
};

} // namespace ev

