#pragma once

#include <QHostAddress>
#include <QString>

class QCoreApplication;

namespace evcs::server {

struct ServerSettings {
    QHostAddress listenAddress;
    quint16 port = 45454;
    QString databasePath;
    QString schemaPath;
    QString logPath;
};

// 读取 JSON 配置和命令行覆盖项，并完成地址、端口与路径校验。
bool parseServerSettings(QCoreApplication &app,
                         ServerSettings *settings,
                         QString *errorMessage);

} // namespace evcs::server
