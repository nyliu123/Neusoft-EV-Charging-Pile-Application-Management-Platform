#include "data/user_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <utility>

namespace ev {

Result<std::optional<UserRecord>> UserRepository::findByPhone(
    QSqlDatabase &database,
    const QString &phone) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT user_id, phone, nickname, avatar_path, "
        "CAST(ROUND(balance * 100) AS INTEGER), register_time, status "
        "FROM users WHERE phone = ? LIMIT 1"));
    query.addBindValue(phone);
    if (!query.exec()) {
        return Result<std::optional<UserRecord>>::fail(ErrorCode::StorageError,
                                                        query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<UserRecord>>::ok(std::nullopt);
    }

    UserRecord user;
    user.userId = query.value(0).toLongLong();
    user.phone = query.value(1).toString();
    user.nickname = query.value(2).toString();
    user.avatarPath = query.value(3).toString();
    user.balanceCent = query.value(4).toLongLong();
    user.registerTime = query.value(5).toString();
    user.status = query.value(6).toString();
    return Result<std::optional<UserRecord>>::ok(std::move(user));
}

Result<UserCreationResult> UserRepository::createAutoRegisteredUser(
    QSqlDatabase &database,
    const QString &phone,
    const QString &nickname,
    const QString &avatarPath) const
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO users (phone, nickname, avatar_path, balance, status) "
        "VALUES (?, ?, ?, 0.00, 'normal')"));
    insert.addBindValue(phone);
    insert.addBindValue(nickname);
    insert.addBindValue(avatarPath);
    if (insert.exec()) {
        UserRecord user;
        user.userId = insert.lastInsertId().toLongLong();
        user.phone = phone;
        user.nickname = nickname;
        user.avatarPath = avatarPath;
        user.balanceCent = 0;
        user.status = QStringLiteral("normal");
        return Result<UserCreationResult>::ok({std::move(user), true});
    }

    // Another request may have inserted the same phone after UML-011's lookup.
    const QString insertError = insert.lastError().text();
    auto existing = findByPhone(database, phone);
    if (existing.success && existing.data.has_value()) {
        return Result<UserCreationResult>::ok({std::move(existing.data.value()), false});
    }
    return Result<UserCreationResult>::fail(ErrorCode::StorageError, insertError);
}

} // namespace ev
