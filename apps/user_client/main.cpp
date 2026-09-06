#include "phone_login_widget.h"
#include "user_api_client.h"
#include "user_home_widget.h"
#include "user_session_state.h"
#include "network/platform_client.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_user_client"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("EV charging platform user client"));
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

    ev::PlatformClient client(QStringLiteral("user_client"));
    QObject::connect(&client, &ev::PlatformClient::stateChanged,
                     [](ev::PlatformClient::State, const QString &detail) {
        qInfo().noquote() << "user client:" << detail;
    });

    if (parser.isSet(checkOption)) {
        QObject::connect(&client, &ev::PlatformClient::healthCheckSucceeded,
                         &application, [&application](const QString &version) {
            qInfo().noquote() << "user client health check passed; server" << version;
            application.exit(0);
        });
        QTimer::singleShot(5000, &application, [&application, &client] {
            if (client.state() != ev::PlatformClient::State::Ready) {
                qCritical() << "user client health check timed out";
                application.exit(2);
            }
        });
        client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
        return application.exec();
    }

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("汽车充电管理平台 用户端"));
    window.resize(1000, 680);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("汽车充电管理平台"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto *pages = new QStackedWidget(central);
    auto *loginPage = new QWidget(pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    auto *loginWidget = new PhoneLoginWidget(loginPage);
    loginLayout->addWidget(loginWidget);
    loginLayout->addStretch();
    auto *userApi = new ev::UserApiClient(&client, &window);
    QObject::connect(userApi, &ev::UserApiClient::sessionExpired, &window,
                     [] {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            modal->close();
        }
    });
    auto *homeWidget = new UserHomeWidget(userApi, pages);
    pages->addWidget(loginPage);
    pages->addWidget(homeWidget);
    layout->addWidget(pages, 1);

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
    QObject::connect(loginWidget, &PhoneLoginWidget::phoneAccepted,
                     &client, &ev::PlatformClient::login);
    QObject::connect(&client, &ev::PlatformClient::loginFailed, &window,
                     [loginWidget](const QString &, const QString &message) {
        loginWidget->showLoginError(message);
    });
    QObject::connect(&client, &ev::PlatformClient::loginSucceeded, &window,
                     [&window, pages, homeWidget, loginWidget](const QJsonObject &userInfo,
                                                              bool isNewUser) {
        if (!UserSessionState::instance().setUserInfo(userInfo)) {
            loginWidget->showLoginError(QStringLiteral("服务端返回的用户信息不完整"));
            return;
        }
        homeWidget->refresh();
        pages->setCurrentWidget(homeWidget);
        homeWidget->showWelcome(isNewUser);
        window.statusBar()->showMessage(
            isNewUser ? QStringLiteral("注册成功，欢迎加入！")
                      : QStringLiteral("登录成功"),
            3000);
    });
    QObject::connect(homeWidget, &UserHomeWidget::logoutRequested,
                     &client, &ev::PlatformClient::logout);
    QObject::connect(&client, &ev::PlatformClient::logoutFinished, &window,
                     [&window, pages, loginPage, loginWidget, homeWidget](
                         bool success, const QString &message) {
        homeWidget->setLogoutInProgress(false);
        if (!success) {
            pages->setCurrentWidget(homeWidget);
            QMessageBox::warning(&window, QStringLiteral("提示"),
                message.isEmpty() ? QStringLiteral("退出登录失败，请稍后重试") : message);
            return;
        }
        UserSessionState::instance().clear();
        loginWidget->resetForLogin();
        pages->setCurrentWidget(loginPage);
        window.statusBar()->showMessage(QStringLiteral("已退出登录"), 3000);
    });
    QObject::connect(&client, &ev::PlatformClient::sessionExpired, &window,
                     [pages, loginPage, loginWidget, homeWidget](const QString &message) {
        UserSessionState::instance().clear();
        homeWidget->setLogoutInProgress(false);
        pages->setCurrentWidget(loginPage);
        loginWidget->showLoginError(message.isEmpty()
            ? QStringLiteral("登录已过期，请重新登录") : message);
    });
    window.show();
    client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
    return application.exec();
}
