#include "data/user_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <utility>

namespace ev {

namespace {

UserRecord recordFromQuery(const QSqlQuery &query)
{
    UserRecord user;
    user.userId = query.value(0).toLongLong();
    user.phone = query.value(1).toString();
    user.nickname = query.value(2).toString();
    user.avatarPath = query.value(3).toString();
    user.balanceCent = query.value(4).toLongLong();
    user.registerTime = query.value(5).toString();
    user.status = query.value(6).toString();
    return user;
}

Result<bool> updateTextField(QSqlDatabase &database, const QString &sql,
                             qint64 userId, const QString &value)
{
    QSqlQuery query(database);
    query.prepare(sql);
    query.addBindValue(value);
    query.addBindValue(userId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return Result<bool>::fail(ErrorCode::NotFound, QStringLiteral("user_not_found"));
    }
    return Result<bool>::ok(true);
}

} // namespace

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

    return Result<std::optional<UserRecord>>::ok(recordFromQuery(query));
}

Result<std::optional<UserRecord>> UserRepository::findById(
    QSqlDatabase &database, qint64 userId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT user_id, phone, nickname, avatar_path, "
        "CAST(ROUND(balance * 100) AS INTEGER), register_time, status "
        "FROM users WHERE user_id = ? LIMIT 1"));
    query.addBindValue(userId);
    if (!query.exec()) {
        return Result<std::optional<UserRecord>>::fail(ErrorCode::StorageError,
                                                        query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<UserRecord>>::ok(std::nullopt);
    }
    return Result<std::optional<UserRecord>>::ok(recordFromQuery(query));
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

Result<bool> UserRepository::updateNickname(QSqlDatabase &database, qint64 userId,
                                             const QString &nickname) const
{
    return updateTextField(database,
        QStringLiteral("UPDATE users SET nickname = ? WHERE user_id = ?"),
        userId, nickname);
}

Result<bool> UserRepository::updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                               const QString &avatarPath) const
{
    return updateTextField(database,
        QStringLiteral("UPDATE users SET avatar_path = ? WHERE user_id = ?"),
        userId, avatarPath);
}

Result<qint64> UserRepository::recharge(QSqlDatabase &database, qint64 userId,
                                        qint64 amountCent) const
{
    if (!database.transaction()) {
        return Result<qint64>::fail(ErrorCode::StorageError,
                                    database.lastError().text());
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE users SET balance = balance + (? / 100.0) WHERE user_id = ?"));
    update.addBindValue(amountCent);
    update.addBindValue(userId);
    if (!update.exec() || update.numRowsAffected() == 0) {
        const QString error = update.lastError().text();
        database.rollback();
        return Result<qint64>::fail(update.numRowsAffected() == 0
                ? ErrorCode::NotFound : ErrorCode::StorageError,
            update.numRowsAffected() == 0 ? QStringLiteral("user_not_found") : error);
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT CAST(ROUND(balance * 100) AS INTEGER) FROM users WHERE user_id = ?"));
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        const QString error = query.lastError().text();
        database.rollback();
        return Result<qint64>::fail(ErrorCode::StorageError, error);
    }
    const qint64 newBalanceCent = query.value(0).toLongLong();
    if (!database.commit()) {
        database.rollback();
        return Result<qint64>::fail(ErrorCode::StorageError,
                                    database.lastError().text());
    }
    return Result<qint64>::ok(newBalanceCent);
}

} // namespace ev
