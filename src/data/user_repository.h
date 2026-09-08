#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace ev {

struct UserRecord {
    qint64 userId = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    qint64 balanceCent = 0;
    QString registerTime;
    QString status;
};

struct UserCreationResult {
    UserRecord user;
    bool created = false;
};

// UML-047: users table data access.
class UserRepository final {
public:
    // ── Query methods ───────────────────────────────────────────────────
    Result<std::optional<UserRecord>> findByPhone(QSqlDatabase &database,
                                                   const QString &phone) const;
    Result<std::optional<UserRecord>> findById(QSqlDatabase &database,
                                                qint64 userId) const;
    // UML-035: registered user count for dashboard.
    Result<int> countAll(QSqlDatabase &database) const;
    // UML-043: full user list ordered by register_time DESC.
    Result<QVector<UserRecord>> listAll(QSqlDatabase &database) const;
    // UML-044: fuzzy search by phone keyword.
    Result<QVector<UserRecord>> searchByPhone(QSqlDatabase &database,
                                              const QString &keyword) const;

    // ── Write methods ──────────────────────────────────────────────────
    Result<UserCreationResult> createAutoRegisteredUser(QSqlDatabase &database,
                                                         const QString &phone,
                                                         const QString &nickname,
                                                         const QString &avatarPath) const;
    Result<bool> updateNickname(QSqlDatabase &database, qint64 userId,
                                const QString &nickname) const;
    Result<bool> updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                  const QString &avatarPath) const;
    Result<bool> updateBalance(QSqlDatabase &database, qint64 userId,
                               qint64 newBalanceCent) const;
    // UML-045: freeze/unfreeze user. status must be 'normal' or 'frozen'.
    Result<bool> updateStatus(QSqlDatabase &database, qint64 userId,
                              const QString &newStatus) const;
    Result<qint64> recharge(QSqlDatabase &database, qint64 userId,
                            qint64 amountCent) const;
};

} // namespace ev
