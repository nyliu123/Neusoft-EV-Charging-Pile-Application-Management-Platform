#include "server_application.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

void applyAiConfiguration(QSettings &settings)
{
    const auto apply = [&settings](const char *environmentName, const char *settingName) {
        const QByteArray value = settings.value(QString::fromLatin1(settingName)).toString().trimmed().toUtf8();
        if (!value.isEmpty()) {
            qputenv(environmentName, value);
        }
    };
    apply("EV_AI_API_KEY", "ai/api_key");
    apply("EV_AI_BASE_URL", "ai/base_url");
    apply("EV_AI_PROVIDER", "ai/provider");
    apply("EV_AI_MODEL", "ai/model");
}

QString defaultDatabasePath()
{
    // Runtime writes must not modify the bundled template or depend on hgfs
    // locking/cache coherence. Explicit --database/config paths still override.
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("ev_charging.sqlite3"));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ev_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("EV charging platform service"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOption({QStringLiteral("c"), QStringLiteral("config")},
                                    QStringLiteral("INI configuration file"),
                                    QStringLiteral("path"));
    QCommandLineOption hostOption(QStringLiteral("host"),
                                  QStringLiteral("TCP listen address (overrides config)"),
                                  QStringLiteral("address"));
    QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("TCP listen port (overrides config)"),
                                  QStringLiteral("port"));
    QCommandLineOption databaseOption({QStringLiteral("d"), QStringLiteral("database")},
                                      QStringLiteral("SQLite path (overrides config)"),
                                      QStringLiteral("path"));
    parser.addOption(configOption);
    parser.addOption(hostOption);
    parser.addOption(portOption);
    parser.addOption(databaseOption);
    parser.process(application);

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 8888;
    QString databasePath = defaultDatabasePath();
    if (parser.isSet(configOption)) {
        QSettings settings(parser.value(configOption), QSettings::IniFormat);
        host = settings.value(QStringLiteral("network/tcp_host"), host).toString();
        port = settings.value(QStringLiteral("network/tcp_port"), port).toUInt();
        databasePath = settings.value(QStringLiteral("database/path"), databasePath).toString();
        applyAiConfiguration(settings);
    }
    if (parser.isSet(hostOption)) {
        host = parser.value(hostOption);
    }
    if (parser.isSet(portOption)) {
        bool validPort = false;
        const uint requestedPort = parser.value(portOption).toUInt(&validPort);
        if (!validPort || requestedPort == 0 || requestedPort > 65535) {
            parser.showHelp(2);
        }
        port = static_cast<quint16>(requestedPort);
    }
    if (parser.isSet(databaseOption)) {
        databasePath = parser.value(databaseOption);
    }

    qInfo().noquote() << "ev_server database:" << databasePath;
    ev::ServerApplication server(databasePath);
    if (!server.start(QHostAddress(host), port)) {
        return 1;
    }
    return application.exec();
}
