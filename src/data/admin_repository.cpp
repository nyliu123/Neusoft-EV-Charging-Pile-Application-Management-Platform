#include "data/admin_repository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ev {

Result<std::optional<AdminRecord>> AdminRepository::findByCredentials(
    QSqlDatabase &database,
    const QString &username,
    const QString &password) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT admin_id, username, create_time "
        "FROM admins WHERE username = ? AND password = ? LIMIT 1"));
    query.addBindValue(username);
    query.addBindValue(password);
    if (!query.exec()) {
        return Result<std::optional<AdminRecord>>::fail(ErrorCode::StorageError,
                                                          query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<AdminRecord>>::ok(std::nullopt);
    }
    AdminRecord admin;
    admin.adminId    = query.value(0).toLongLong();
    admin.username   = query.value(1).toString();
    admin.createTime = query.value(2).toString();
    return Result<std::optional<AdminRecord>>::ok(admin);
}

} // namespace ev
