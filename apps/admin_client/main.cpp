#include "network/platform_client.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_admin_client"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("汽车充电管理平台 管理端"));
    window.resize(1100, 720);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("运营管理中心"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    layout->addWidget(new QLabel(
        QStringLiteral("管理端主干已就绪：经营看板、站点、设备、用户和订单页面将在功能分支接入。"),
        central));
    auto *connectionLabel = new QLabel(QStringLiteral("连接状态：正在连接"), central);
    auto *retryButton = new QPushButton(QStringLiteral("重新连接"), central);
    layout->addWidget(connectionLabel);
    layout->addWidget(retryButton, 0, Qt::AlignLeft);
    layout->addStretch();
    window.setCentralWidget(central);
    QObject::connect(&client, &ev::PlatformClient::stateChanged, &window,
                     [&window, connectionLabel](ev::PlatformClient::State state,
                                                const QString &detail) {
        connectionLabel->setText(QStringLiteral("连接状态：%1").arg(detail));
        window.statusBar()->showMessage(
            state == ev::PlatformClient::State::Ready
                ? QStringLiteral("服务端可用") : detail);
    });
    QObject::connect(retryButton, &QPushButton::clicked,
                     &client, &ev::PlatformClient::reconnectNow);
    window.show();
    client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
    return application.exec();
}
