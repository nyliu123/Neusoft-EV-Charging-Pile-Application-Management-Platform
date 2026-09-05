#pragma once

#include "common/result.h"

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
};

} // namespace ev
