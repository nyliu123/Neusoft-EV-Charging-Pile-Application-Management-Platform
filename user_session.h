#pragma once

#include <QJsonObject>
#include <QString>

namespace evcs::userclient {

class UserSession final
{
public:
    static UserSession &instance();

    void setAuthenticated(const QString &token, const QJsonObject &user);
    void updateUser(const QJsonObject &user);
    void clear();

    bool isLoggedIn() const;
    QString token() const;
    QJsonObject user() const;

private:
    UserSession() = default;

    QString token_;
    QJsonObject user_;
};

} // namespace evcs::userclient
