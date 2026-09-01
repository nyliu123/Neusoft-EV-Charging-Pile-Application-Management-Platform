#include "chargingserver.h"
#include "logging.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

struct ServerSettings {
    QString listenAddress = QStringLiteral("0.0.0.0");
    int port = 45454;
    QString databasePath = QStringLiteral("data/evcharging.db");
    QString schemaPath = QStringLiteral("database/schema.sql");
    QString logPath = QStringLiteral("logs/evcs-server.jsonl");
};

bool loadSettings(const QString &path, ServerSettings *settings, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("无法读取配置文件 %1：%2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = QStringLiteral("配置文件 JSON 无效：%1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject object = document.object();
    settings->listenAddress = object.value(QStringLiteral("listenAddress")).toString(settings->listenAddress);
    settings->port = object.value(QStringLiteral("port")).toInt(settings->port);
    settings->databasePath = object.value(QStringLiteral("databasePath")).toString(settings->databasePath);
    settings->schemaPath = object.value(QStringLiteral("schemaPath")).toString(settings->schemaPath);
    settings->logPath = object.value(QStringLiteral("logPath")).toString(settings->logPath);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("evcs_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("电动汽车充电桩应用管理平台服务端"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOption(
        {QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("JSON 配置文件"), QStringLiteral("path"), QStringLiteral("config/server.json"));
    parser.addOption(configOption);
    const QCommandLineOption listenOption(
        {QStringLiteral("l"), QStringLiteral("listen")},
        QStringLiteral("监听地址（覆盖配置文件）"), QStringLiteral("address"));
    parser.addOption(listenOption);
    const QCommandLineOption portOption(
        {QStringLiteral("p"), QStringLiteral("port")},
        QStringLiteral("监听端口（覆盖配置文件）"), QStringLiteral("port"));
    parser.addOption(portOption);
    const QCommandLineOption databaseOption(
        {QStringLiteral("d"), QStringLiteral("database")},
        QStringLiteral("SQLite 数据库文件（覆盖配置文件）"), QStringLiteral("path"));
    parser.addOption(databaseOption);
    const QCommandLineOption schemaOption(
        QStringLiteral("schema"), QStringLiteral("数据库结构 SQL 文件（覆盖配置文件）"),
        QStringLiteral("path"));
    parser.addOption(schemaOption);
    const QCommandLineOption logOption(
        QStringLiteral("log"), QStringLiteral("JSON Lines 日志文件（覆盖配置文件）"),
        QStringLiteral("path"));
    parser.addOption(logOption);
    parser.process(app);

    ServerSettings settings;
    QString errorMessage;
    if (!loadSettings(parser.value(configOption), &settings, &errorMessage)) {
        fprintf(stderr, "%s\n", qPrintable(errorMessage));
        return 2;
    }
    if (parser.isSet(listenOption)) settings.listenAddress = parser.value(listenOption);
    if (parser.isSet(databaseOption)) settings.databasePath = parser.value(databaseOption);
    if (parser.isSet(schemaOption)) settings.schemaPath = parser.value(schemaOption);
    if (parser.isSet(logOption)) settings.logPath = parser.value(logOption);
    bool portOk = false;
    if (parser.isSet(portOption)) settings.port = parser.value(portOption).toInt(&portOk);
    else portOk = true;
    QHostAddress listenAddress;
    if (!portOk || settings.port < 1 || settings.port > 65535
        || !listenAddress.setAddress(settings.listenAddress)) {
        fprintf(stderr, "无效监听地址或端口\n");
        return 2;
    }
    if (!evcs::server::installStructuredLogging(settings.logPath, &errorMessage)) {
        fprintf(stderr, "%s\n", qPrintable(errorMessage));
        return 2;
    }

    evcs::server::ChargingServer server;
    if (!server.initialize(settings.databasePath, settings.schemaPath, &errorMessage)) {
        qCritical().noquote() << QStringLiteral("database_initialization_failed detail=%1").arg(errorMessage);
        return 1;
    }
    if (!server.listen(listenAddress, static_cast<quint16>(settings.port), &errorMessage)) {
        qCritical().noquote() << QStringLiteral("server_start_failed detail=%1").arg(errorMessage);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("server_started address=%1 port=%2 database=%3")
                            .arg(settings.listenAddress)
                            .arg(server.serverPort())
                            .arg(settings.databasePath);
    return app.exec();
}
