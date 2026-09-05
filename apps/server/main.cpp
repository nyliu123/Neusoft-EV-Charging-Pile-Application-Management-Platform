#include "server_application.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

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
    parser.addOption(configOption);
    parser.process(application);

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 8888;
    QString databasePath = QDir::current().absoluteFilePath(
        QStringLiteral("data/ev_charging.sqlite3"));
    if (parser.isSet(configOption)) {
        QSettings settings(parser.value(configOption), QSettings::IniFormat);
        host = settings.value(QStringLiteral("network/tcp_host"), host).toString();
        port = settings.value(QStringLiteral("network/tcp_port"), port).toUInt();
        databasePath = settings.value(QStringLiteral("database/path"), databasePath).toString();
    }

    ev::ServerApplication server(databasePath);
    if (!server.start(QHostAddress(host), port)) {
        return 1;
    }
    return application.exec();
}

