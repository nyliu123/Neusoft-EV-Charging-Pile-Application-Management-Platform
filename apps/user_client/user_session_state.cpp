#include "user_session_state.h"

#include <QtMath>

UserSessionState &UserSessionState::instance()
{
    static UserSessionState state;
    return state;
}

bool UserSessionState::setUserInfo(const QJsonObject &userInfo)
{
    const qint64 userId = userInfo.value(QStringLiteral("user_id")).toInteger();
    const QString sessionId = userInfo.value(QStringLiteral("session_id")).toString();
    if (userId <= 0 || sessionId.isEmpty()) {
        clear();
        return false;
    }

    userId_ = userId;
    nickname_ = userInfo.value(QStringLiteral("nickname")).toString();
    avatarPath_ = userInfo.value(QStringLiteral("avatar_path")).toString();
    balanceCent_ = qRound64(
        userInfo.value(QStringLiteral("balance")).toDouble() * 100.0);
    sessionId_ = sessionId;
    loggedIn_ = true;
    return true;
}

void UserSessionState::clear()
{
    loggedIn_ = false;
    userId_ = 0;
    nickname_.clear();
    avatarPath_.clear();
    balanceCent_ = 0;
    sessionId_.clear();
}

bool UserSessionState::isLoggedIn() const
{
    return loggedIn_ && userId_ > 0 && !sessionId_.isEmpty();
}
qint64 UserSessionState::userId() const { return userId_; }
QString UserSessionState::nickname() const { return nickname_; }
QString UserSessionState::avatarPath() const { return avatarPath_; }
qint64 UserSessionState::balanceCent() const { return balanceCent_; }
QString UserSessionState::sessionId() const { return sessionId_; }
