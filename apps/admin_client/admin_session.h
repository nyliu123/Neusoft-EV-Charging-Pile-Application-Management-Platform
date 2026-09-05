#pragma once

#include <QString>

namespace ev {

struct AdminSession {
    int adminId = 0;
    QString username;
    QString sessionId;
    bool isLoggedIn = false;
};

} // namespace ev
