#include "services/admin_seeder.h"

#include "common/password_hasher.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringLiteral>

namespace ev {

Result<int> AdminSeeder::seedIfNeeded(QSqlDatabase &database)
{
    // Check if any admin exists.
    QSqlQuery countQuery(database);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM admins"))) {
        return Result<int>::fail(ErrorCode::StorageError, countQuery.lastError().text());
    }
    if (!countQuery.next()) {
        return Result<int>::fail(ErrorCode::InternalError,
                                 QStringLiteral("failed to check admin count"));
    }
    const int adminCount = countQuery.value(0).toInt();
    if (adminCount > 0) {
        return Result<int>::ok(0);
    }

    // Generate password hash using the same hasher used for verification.
    const PasswordHash hash = PasswordHasher::hash(QString::fromLatin1(kDefaultPassword));
    const QString storedHash = PasswordHasher::serialize(hash);

    // Insert default admin.
    QSqlQuery insertQuery(database);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO admins (username, password) VALUES (?, ?)"));
    insertQuery.addBindValue(QString::fromLatin1(kDefaultUsername));
    insertQuery.addBindValue(storedHash);

    if (!insertQuery.exec()) {
        return Result<int>::fail(ErrorCode::StorageError, insertQuery.lastError().text());
    }

    return Result<int>::ok(1);
}

} // namespace ev
