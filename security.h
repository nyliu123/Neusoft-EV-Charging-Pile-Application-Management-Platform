#pragma once

#include <QString>

namespace evcs::server::security {

QString createSalt();
QString hashPassword(const QString &password, const QString &salt);
bool verifyPassword(const QString &password,
                    const QString &salt,
                    const QString &expectedHash);

} // namespace evcs::server::security
