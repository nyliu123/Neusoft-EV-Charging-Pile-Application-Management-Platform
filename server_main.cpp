#include "chargingserver.h"
#include "logging.h"
#include "server_settings.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("evcs_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.2.0"));

    // 主函数只负责装配：解析配置、启动日志、初始化数据库并监听端口。
    evcs::server::ServerSettings settings;
    QString errorMessage;
    if (!evcs::server::parseServerSettings(app, &settings, &errorMessage)
        || !evcs::server::installStructuredLogging(settings.logPath, &errorMessage)) {
        fprintf(stderr, "%s\n", qPrintable(errorMessage));
        return 2;
    }

    evcs::server::ChargingServer server;
    server.configureMap(settings.tencentMapKey, settings.tencentMapReferer);
    if (!server.initialize(settings.databasePath, settings.schemaPath, &errorMessage)
        || !server.listen(settings.listenAddress, settings.port, &errorMessage)) {
        qCritical().noquote() << QStringLiteral("服务端启动失败：%1").arg(errorMessage);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("服务端已启动，端口=%1，数据库=%2")
                            .arg(server.serverPort())
                            .arg(settings.databasePath);
    return app.exec();
}
