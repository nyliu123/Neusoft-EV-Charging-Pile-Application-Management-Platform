#pragma once

#include "common/result.h"
#include "data/user_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace ev {

struct LoginUserInfo {
    qint64 userId = 0;
    QString nickname;
    QString avatarPath;
    qint64 balanceCent = 0;
    QString sessionId;
    bool isNewUser = false;
};

class UserService final {
public:
    Result<LoginUserInfo> loginExistingUser(QSqlDatabase &database,
                                             const QString &phone) const;
    Result<LoginUserInfo> registerAutomatically(QSqlDatabase &database,
                                                 const QString &phone) const;
    Result<UserRecord> queryUserInfo(QSqlDatabase &database, qint64 userId) const;
    Result<bool> updateNickname(QSqlDatabase &database, qint64 userId,
                                const QString &nickname) const;
    Result<bool> updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                  const QString &avatarPath) const;
    Result<qint64> recharge(QSqlDatabase &database, qint64 userId,
                            qint64 amountCent) const;
};

} // namespace ev
