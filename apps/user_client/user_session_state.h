#pragma once

#include <QJsonObject>
#include <QString>

class UserSessionState final {
public:
    static UserSessionState &instance();

    bool setUserInfo(const QJsonObject &userInfo);
    void clear();

    bool isLoggedIn() const;
    qint64 userId() const;
    QString nickname() const;
    QString avatarPath() const;
    qint64 balanceCent() const;
    QString sessionId() const;

private:
    UserSessionState() = default;

    bool loggedIn_ = false;
    qint64 userId_ = 0;
    QString nickname_;
    QString avatarPath_;
    qint64 balanceCent_ = 0;
    QString sessionId_;
};
