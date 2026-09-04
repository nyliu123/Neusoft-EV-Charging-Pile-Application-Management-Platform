#pragma once

#include <QHostAddress>
#include <QString>

class QCoreApplication;

namespace evcs::server {

struct ServerSettings {
    QHostAddress listenAddress;
    quint16 port = 8888;
    QString databasePath;
    QString schemaPath;
    QString logPath;
    QString tencentMapKey;
    QString tencentMapReferer = QStringLiteral("EVCS-DEMO");
};

// 读取 JSON 配置和命令行覆盖项，并完成地址、端口与路径校验。
bool parseServerSettings(QCoreApplication &app,
                         ServerSettings *settings,
                         QString *errorMessage);

} // namespace evcs::server
