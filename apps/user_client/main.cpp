#include "phone_login_widget.h"
#include "user_api_client.h"
#include "user_home_widget.h"
#include "user_session_state.h"
#include "client_ui/client_style.h"
#include "network/platform_client.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QMainWindow>
#include <QMessageBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    ev::configureClientInputMethod();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_user_client"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    ev::installClientStyle(application);
    QFile styleFile(QStringLiteral(":/styles/client.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    } else {
        qWarning() << "cannot load global client stylesheet";
    }

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
    window.setObjectName(QStringLiteral("userClientWindow"));
    window.setWindowTitle(QStringLiteral("汽车充电管理平台 用户端"));
    window.resize(460, 380);

    auto *central = new QWidget(&window);
    central->setObjectName(QStringLiteral("userClientShell"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *pages = new QStackedWidget(central);
    auto *loginPage = new QWidget(pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->addStretch();
    auto *loginWidget = new PhoneLoginWidget(loginPage);
    loginLayout->addWidget(loginWidget, 0, Qt::AlignCenter);
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

    window.setCentralWidget(central);
    QObject::connect(&client, &ev::PlatformClient::stateChanged, &window,
                     [&window](ev::PlatformClient::State state, const QString &detail) {
        window.statusBar()->showMessage(
            state == ev::PlatformClient::State::Ready
                ? QStringLiteral("服务端可用") : detail);
    });
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
        window.resize(1000, 680);
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
        window.resize(460, 380);
        window.statusBar()->showMessage(QStringLiteral("已退出登录"), 3000);
    });
    QObject::connect(&client, &ev::PlatformClient::sessionExpired, &window,
                     [&window, pages, loginPage, loginWidget, homeWidget](const QString &message) {
        UserSessionState::instance().clear();
        homeWidget->setLogoutInProgress(false);
        pages->setCurrentWidget(loginPage);
        window.resize(460, 380);
        loginWidget->showLoginError(message.isEmpty()
            ? QStringLiteral("登录已过期，请重新登录") : message);
    });
    window.show();
    client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
    return application.exec();
}
