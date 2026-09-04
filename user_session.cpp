#include "user_session.h"

namespace evcs::userclient {

UserSession &UserSession::instance()
{
    static UserSession session;
    return session;
}

void UserSession::setAuthenticated(const QString &token, const QJsonObject &user)
{
    token_ = token;
    user_ = user;
}

void UserSession::updateUser(const QJsonObject &user)
{
    for (auto it = user.begin(); it != user.end(); ++it) user_.insert(it.key(), it.value());
}

void UserSession::clear()
{
    token_.clear();
    user_ = {};
}

bool UserSession::isLoggedIn() const
{
    return !token_.isEmpty() && !user_.isEmpty();
}

QString UserSession::token() const
{
    return token_;
}

QJsonObject UserSession::user() const
{
    return user_;
}

} // namespace evcs::userclient
