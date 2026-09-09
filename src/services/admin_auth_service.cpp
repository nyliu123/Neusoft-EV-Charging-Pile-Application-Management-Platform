#include "services/admin_auth_service.h"

#include "common/password_hasher.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ev {

Result<AdminInfo> AdminAuthService::authenticate(const QString &username,
                                                  const QString &password,
                                                  QSqlDatabase &database)
{
    // Validate input.
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        return Result<AdminInfo>::fail(ErrorCode::InvalidInput,
                                       QStringLiteral("username and password are required"));
    }

    // Query admin by username.
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT admin_id, username, password FROM admins WHERE username = ?"));
    query.addBindValue(username.trimmed());

    if (!query.exec()) {
        return Result<AdminInfo>::fail(ErrorCode::StorageError, query.lastError().text());
    }

    if (!query.next()) {
        // Username not found — return generic error to avoid user enumeration.
        return Result<AdminInfo>::fail(ErrorCode::Unauthorized,
                                       QStringLiteral("invalid username or password"));
    }

    const int adminId = query.value(0).toInt();
    const QString dbUsername = query.value(1).toString();
    const QString storedHash = query.value(2).toString();

    // Deserialize and verify password.
    PasswordHash ph;
    if (!PasswordHasher::deserialize(storedHash, ph)) {
        return Result<AdminInfo>::fail(ErrorCode::InternalError,
                                       QStringLiteral("stored password hash is corrupted"));
    }

    if (!PasswordHasher::verify(password, ph)) {
        return Result<AdminInfo>::fail(ErrorCode::Unauthorized,
                                       QStringLiteral("invalid username or password"));
    }

    AdminInfo info;
    info.adminId = adminId;
    info.username = dbUsername;
    return Result<AdminInfo>::ok(info);
}

Result<bool> AdminAuthService::verifyPassword(qint64 adminId, const QString &password,
                                               QSqlDatabase &database)
{
    if (adminId <= 0 || password.isEmpty()) {
        return Result<bool>::fail(ErrorCode::InvalidInput,
                                  QStringLiteral("请输入管理员密码"));
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT password FROM admins WHERE admin_id = ?"));
    query.addBindValue(adminId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (!query.next()) {
        return Result<bool>::fail(ErrorCode::Unauthorized,
                                  QStringLiteral("管理员身份无效"));
    }
    PasswordHash hash;
    if (!PasswordHasher::deserialize(query.value(0).toString(), hash)) {
        return Result<bool>::fail(ErrorCode::InternalError,
                                  QStringLiteral("管理员密码数据损坏"));
    }
    if (!PasswordHasher::verify(password, hash)) {
        return Result<bool>::fail(ErrorCode::Unauthorized,
                                  QStringLiteral("管理员密码错误"));
    }
    return Result<bool>::ok(true);
}

} // namespace ev
