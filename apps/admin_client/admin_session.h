#pragma once

#include <QString>

namespace ev {

struct AdminSession {
    int adminId = 0;
    QString username;
    bool isLoggedIn = false;
};

} // namespace ev
