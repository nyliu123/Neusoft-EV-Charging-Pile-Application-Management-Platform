#include "admin_login_dialog.h"
#include "admin_main_window.h"
#include "admin_session.h"
#include "client_ui/client_style.h"
#include "network/platform_client.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char *argv[])
{
    ev::configureClientInputMethod();
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_admin_client"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    ev::installClientStyle(application);
    QFile styleFile(QStringLiteral(":/styles/client.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    } else {
        qWarning() << "cannot load global client stylesheet";
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("EV charging platform admin client"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hostOption(QStringLiteral("host"), QStringLiteral("Server address"),
                                  QStringLiteral("address"), QStringLiteral("127.0.0.1"));
    QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Server TCP port"), QStringLiteral("port"),
                                  QStringLiteral("8888"));
    QCommandLineOption checkOption(QStringLiteral("check"),
                                   QStringLiteral("Exit after a successful health check"));
    parser.addOptions({hostOption, portOption, checkOption});
    parser.process(application);

    bool validPort = false;
    const uint requestedPort = parser.value(portOption).toUInt(&validPort);
    if (!validPort || requestedPort == 0 || requestedPort > 65535) {
        parser.showHelp(2);
    }

    ev::PlatformClient client(QStringLiteral("admin_client"));
    QObject::connect(&client, &ev::PlatformClient::stateChanged,
                     [](ev::PlatformClient::State, const QString &detail) {
        qInfo().noquote() << "admin client:" << detail;
    });

    // --check mode: exit after health check.
    if (parser.isSet(checkOption)) {
        QObject::connect(&client, &ev::PlatformClient::healthCheckSucceeded,
                         &application, [&application](const QString &version) {
            qInfo().noquote() << "admin client health check passed; server" << version;
            application.exit(0);
        });
        QTimer::singleShot(5000, &application, [&application, &client] {
            if (client.state() != ev::PlatformClient::State::Ready) {
                qCritical() << "admin client health check timed out";
                application.exit(2);
            }
        });
        client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
        return application.exec();
    }

    // Normal mode: connect, then show login dialog, then main window.
    client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));

    // Wait for connection to be ready before showing login dialog.
    // Show dialog immediately, but disable login button until ready.
    ev::AdminLoginDialog loginDialog(&client);
    const int dialogResult = loginDialog.exec();

    if (dialogResult != QDialog::Accepted) {
        return 0;
    }

    ev::AdminMainWindow mainWindow(&client, loginDialog.session());
    mainWindow.show();

    return application.exec();
}
