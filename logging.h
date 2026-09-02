#pragma once

#include <QString>

namespace evcs::server {

bool installStructuredLogging(const QString &logPath, QString *errorMessage = nullptr);

} // namespace evcs::server
