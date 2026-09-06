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

bool UserSessionState::updateUserInfo(const QJsonObject &userInfo)
{
    if (!isLoggedIn()) {
        return false;
    }
    const qint64 returnedUserId = userInfo.value(QStringLiteral("user_id")).toInteger(userId_);
    if (returnedUserId != userId_) {
        return false;
    }
    if (userInfo.contains(QStringLiteral("nickname"))) {
        nickname_ = userInfo.value(QStringLiteral("nickname")).toString();
    }
    if (userInfo.contains(QStringLiteral("avatar_path"))) {
        avatarPath_ = userInfo.value(QStringLiteral("avatar_path")).toString();
    }
    if (userInfo.contains(QStringLiteral("balance_cent"))) {
        balanceCent_ = userInfo.value(QStringLiteral("balance_cent")).toInteger();
    } else if (userInfo.contains(QStringLiteral("balance"))) {
        balanceCent_ = qRound64(userInfo.value(QStringLiteral("balance")).toDouble() * 100.0);
    }
    return true;
}

void UserSessionState::setNickname(const QString &nickname) { nickname_ = nickname; }
void UserSessionState::setAvatarPath(const QString &avatarPath) { avatarPath_ = avatarPath; }
void UserSessionState::setBalanceCent(qint64 balanceCent) { balanceCent_ = balanceCent; }

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
