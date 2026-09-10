#include "phone_login_widget.h"
#include "user_api_client.h"
#include "user_home_widget.h"
#include "user_session_state.h"
#include "client_ui/client_style.h"
#include "client_ui/item_full_text_filter.h"
#include "client_ui/apple_widgets.h"
#include "network/platform_client.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMessageBox>
#include <QStatusBar>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class MobileDialogFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *dialog = qobject_cast<QDialog *>(watched);
        if (!dialog || event->type() != QEvent::Show || !dialog->isWindow()) {
            return QObject::eventFilter(watched, event);
        }
        QTimer::singleShot(0, dialog, [dialog] {
            QWidget *owner = dialog->parentWidget();
            const QSize ownerSize = owner ? owner->window()->size() : QSize(430, 800);
            const QSize limit(qMax(280, ownerSize.width() - 24),
                              qMax(180, ownerSize.height() - 48));
            dialog->setMinimumSize(0, 0);
            dialog->setMaximumSize(limit);
            if (auto *message = qobject_cast<QMessageBox *>(dialog)) {
                for (QLabel *label : message->findChildren<QLabel *>()) {
                    if (label->objectName() == QStringLiteral("qt_msgbox_label")) {
                        const int textWidth = qMax(180, limit.width() - 100);
                        QString wrappedText;
                        int lineWidth = 0;
                        for (const QChar character : label->text()) {
                            if (character == QLatin1Char('\n')) {
                                wrappedText.append(character);
                                lineWidth = 0;
                                continue;
                            }
                            const int characterWidth = label->fontMetrics().horizontalAdvance(character);
                            if (lineWidth > 0 && lineWidth + characterWidth > textWidth) {
                                wrappedText.append(QLatin1Char('\n'));
                                lineWidth = 0;
                            }
                            wrappedText.append(character);
                            lineWidth += characterWidth;
                        }
                        label->setTextFormat(Qt::PlainText);
                        label->setText(wrappedText);
                        label->setWordWrap(true);
                        const int textHeight = label->fontMetrics().boundingRect(
                            QRect(0, 0, textWidth, limit.height()), Qt::TextWordWrap,
                            label->text()).height() + 8;
                        label->setFixedSize(textWidth, textHeight);
                        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                    }
                }
                message->layout()->setSizeConstraint(QLayout::SetNoConstraint);
                message->layout()->invalidate();
                message->layout()->activate();
                message->setFixedWidth(limit.width());
                message->resize(limit.width(),
                    qMin(message->layout()->sizeHint().height(), limit.height()));
            } else {
                dialog->resize(dialog->size().boundedTo(limit));
            }
        });
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    ev::configureClientInputMethod();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    MobileDialogFilter mobileDialogFilter(&application);
    application.installEventFilter(&mobileDialogFilter);
    QApplication::setApplicationName(QStringLiteral("ev_user_client"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    ev::installClientStyle(application);
    ev::installItemFullTextFilter(application);
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
    window.setWindowTitle(QStringLiteral("轻充 · 用户端"));
    const QSize phoneSize(430, 800);
    const QSize available = window.screen()->availableGeometry().size() - QSize(20, 48);
    window.resize(phoneSize.boundedTo(available));

    auto *central = new QWidget(&window);
    central->setObjectName(QStringLiteral("userClientShell"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *pages = new QStackedWidget(central);
    auto *loginPage = new QWidget(pages);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(16, 20, 16, 20);
    auto *loginScroll = new QScrollArea(loginPage);
    loginScroll->setWidgetResizable(true);
    loginScroll->setFrameShape(QFrame::NoFrame);
    auto *loginArea = new QWidget(loginScroll);
    auto *formLayout = new QVBoxLayout(loginArea);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->addStretch(2);
    formLayout->addWidget(ev::makeAppleBrand(loginArea), 0, Qt::AlignHCenter);
    formLayout->addSpacing(16);
    auto *loginWidget = new PhoneLoginWidget(loginArea);
    formLayout->addWidget(loginWidget);
    formLayout->addStretch(1);
    loginScroll->setWidget(loginArea);
    loginLayout->addWidget(loginScroll);
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
        Q_UNUSED(state)
        window.statusBar()->showMessage(detail);
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
                     [&window, pages, loginPage, loginWidget, homeWidget](const QString &message) {
        UserSessionState::instance().clear();
        homeWidget->setLogoutInProgress(false);
        pages->setCurrentWidget(loginPage);
        loginWidget->showLoginError(message.isEmpty()
            ? QStringLiteral("登录已过期，请重新登录") : message);
    });
    window.show();
    ev::centerClientWindowOnScreen(window);
    client.connectToServer(parser.value(hostOption), static_cast<quint16>(requestedPort));
    return application.exec();
}
