#include "admin_session.h"

namespace evcs::adminclient {

AdminSession &AdminSession::instance()
{
    static AdminSession session;
    return session;
}

void AdminSession::setAuthenticated(const QString &token, const QJsonObject &admin)
{
    token_ = token;
    admin_ = admin;
}

void AdminSession::clear() { token_.clear(); admin_ = {}; }
bool AdminSession::isLoggedIn() const { return !token_.isEmpty() && !admin_.isEmpty(); }
QString AdminSession::token() const { return token_; }
QJsonObject AdminSession::admin() const { return admin_; }

} // namespace evcs::adminclient
