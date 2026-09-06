#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>
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

class UserRepository final {
public:
    Result<std::optional<UserRecord>> findByPhone(QSqlDatabase &database,
                                                   const QString &phone) const;
    Result<std::optional<UserRecord>> findById(QSqlDatabase &database,
                                                qint64 userId) const;
    Result<UserCreationResult> createAutoRegisteredUser(QSqlDatabase &database,
                                                         const QString &phone,
                                                         const QString &nickname,
                                                         const QString &avatarPath) const;
    Result<bool> updateNickname(QSqlDatabase &database, qint64 userId,
                                const QString &nickname) const;
    Result<bool> updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                  const QString &avatarPath) const;
    Result<qint64> recharge(QSqlDatabase &database, qint64 userId,
                            qint64 amountCent) const;
};

} // namespace ev
