#pragma once

#include <QJsonObject>
#include <QString>

namespace evcs::adminclient {

class AdminSession final
{
public:
    static AdminSession &instance();
    void setAuthenticated(const QString &token, const QJsonObject &admin);
    void clear();
    bool isLoggedIn() const;
    QString token() const;
    QJsonObject admin() const;

private:
    AdminSession() = default;
    QString token_;
    QJsonObject admin_;
};

} // namespace evcs::adminclient
