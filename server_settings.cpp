#include "server_settings.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace evcs::server {
namespace {

struct RawSettings {
    QString listenAddress = QStringLiteral("0.0.0.0");
    int port = 8888;
    QString databasePath = QStringLiteral("evcharging.db");
    QString schemaPath = QStringLiteral("schema.sql");
    QString logPath = QStringLiteral("evcs-server.jsonl");
    QString tencentMapKey;
    QString tencentMapReferer = QStringLiteral("EVCS-DEMO");
};

// 配置文件只负责提供默认值，命令行参数具有更高优先级。
bool loadJsonSettings(const QString &path, RawSettings *settings, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取配置文件 %1：%2")
                                .arg(path, file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("配置文件 JSON 无效：%1")
                                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject object = document.object();
    settings->listenAddress = object.value(QStringLiteral("listenAddress"))
                                  .toString(settings->listenAddress);
    settings->port = object.value(QStringLiteral("port")).toInt(settings->port);
    settings->databasePath = object.value(QStringLiteral("databasePath"))
                                 .toString(settings->databasePath);
    settings->schemaPath = object.value(QStringLiteral("schemaPath"))
                               .toString(settings->schemaPath);
    settings->logPath = object.value(QStringLiteral("logPath"))
                            .toString(settings->logPath);
    settings->tencentMapKey = object.value(QStringLiteral("tencentMapKey"))
                                  .toString(settings->tencentMapKey);
    settings->tencentMapReferer = object.value(QStringLiteral("tencentMapReferer"))
                                      .toString(settings->tencentMapReferer);
    return true;
}

} // namespace

bool parseServerSettings(QCoreApplication &app,
                         ServerSettings *settings,
                         QString *errorMessage)
{
    if (!settings) {
        if (errorMessage) *errorMessage = QStringLiteral("服务端配置输出对象不能为空");
        return false;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("电动汽车充电桩应用管理平台服务端"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOption(
        {QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("JSON 配置文件"), QStringLiteral("path"), QStringLiteral("server.json"));
    const QCommandLineOption listenOption(
        {QStringLiteral("l"), QStringLiteral("listen")},
        QStringLiteral("监听地址（覆盖配置文件）"), QStringLiteral("address"));
    const QCommandLineOption portOption(
        {QStringLiteral("p"), QStringLiteral("port")},
        QStringLiteral("监听端口（覆盖配置文件）"), QStringLiteral("port"));
    const QCommandLineOption databaseOption(
        {QStringLiteral("d"), QStringLiteral("database")},
        QStringLiteral("SQLite 数据库文件（覆盖配置文件）"), QStringLiteral("path"));
    const QCommandLineOption schemaOption(
        QStringLiteral("schema"), QStringLiteral("数据库结构 SQL 文件（覆盖配置文件）"),
        QStringLiteral("path"));
    const QCommandLineOption logOption(
        QStringLiteral("log"), QStringLiteral("JSON Lines 日志文件（覆盖配置文件）"),
        QStringLiteral("path"));
    parser.addOptions({configOption, listenOption, portOption, databaseOption,
                       schemaOption, logOption});
    parser.process(app);

    RawSettings raw;
    if (!loadJsonSettings(parser.value(configOption), &raw, errorMessage)) return false;
    if (parser.isSet(listenOption)) raw.listenAddress = parser.value(listenOption);
    if (parser.isSet(databaseOption)) raw.databasePath = parser.value(databaseOption);
    if (parser.isSet(schemaOption)) raw.schemaPath = parser.value(schemaOption);
    if (parser.isSet(logOption)) raw.logPath = parser.value(logOption);

    bool portOk = true;
    if (parser.isSet(portOption)) raw.port = parser.value(portOption).toInt(&portOk);
    QHostAddress address;
    if (!portOk || raw.port < 1 || raw.port > 65535
        || !address.setAddress(raw.listenAddress)) {
        if (errorMessage) *errorMessage = QStringLiteral("监听地址或端口无效");
        return false;
    }

    settings->listenAddress = address;
    settings->port = static_cast<quint16>(raw.port);
    settings->databasePath = raw.databasePath;
    settings->schemaPath = raw.schemaPath;
    settings->logPath = raw.logPath;
    settings->tencentMapKey = qEnvironmentVariable("EVCS_TENCENT_MAP_KEY", raw.tencentMapKey);
    settings->tencentMapReferer = qEnvironmentVariable(
        "EVCS_TENCENT_MAP_REFERER", raw.tencentMapReferer);
    return true;
}

} // namespace evcs::server
