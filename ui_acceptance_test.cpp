#include "admin_mainwindow.h"
#include "chargingserver.h"
#include "user_mainwindow.h"

#include <QDir>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class UiAcceptanceTest final : public QObject
{
    Q_OBJECT

private slots:
    void clientsLoginLoadDataAndRender();
};

void UiAcceptanceTest::clientsLoginLoadDataAndRender()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    evcs::server::ChargingServer server;
    QString error;
    QVERIFY2(server.initialize(temporaryDirectory.filePath(QStringLiteral("ui.db")),
                               QStringLiteral(EVCS_TEST_SCHEMA_PATH), &error),
             qPrintable(error));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));

    const QString artifactDirectory = qEnvironmentVariable(
        "EVCS_TEST_ARTIFACT_DIR", temporaryDirectory.filePath(QStringLiteral("artifacts")));
    QVERIFY(QDir().mkpath(artifactDirectory));

    {
        evcs::userclient::MainWindow window;
        window.show();
        QVERIFY2(!window.styleSheet().trimmed().isEmpty(), "用户端 QSS 资源未加载");
        QVERIFY(window.findChild<QWidget *>(QStringLiteral("userStationPage")));
        QVERIFY(window.findChild<QWidget *>(QStringLiteral("userProfilePage")));
        auto *port = window.findChild<QSpinBox *>(QStringLiteral("serverPort"));
        auto *connectButton = window.findChild<QPushButton *>(QStringLiteral("connectButton"));
        auto *loginButton = window.findChild<QPushButton *>(QStringLiteral("loginButton"));
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("userStack"));
        auto *stations = window.findChild<QTableWidget *>(QStringLiteral("stationTable"));
        QVERIFY(port && connectButton && loginButton && stack && stations);
        port->setValue(server.serverPort());
        QTest::mouseClick(connectButton, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(loginButton->isEnabled(), 3000);
        QTest::mouseClick(loginButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(stack->currentIndex(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(stations->rowCount() >= 3, 3000);
        QVERIFY(window.grab().save(QDir(artifactDirectory).filePath(
            QStringLiteral("user-client-acceptance.png"))));
        window.close();
    }
    QCoreApplication::processEvents();

    {
        evcs::adminclient::MainWindow window;
        window.show();
        QVERIFY2(!window.styleSheet().trimmed().isEmpty(), "管理端 QSS 资源未加载");
        QVERIFY(window.findChild<QWidget *>(QStringLiteral("adminDashboardPage")));
        QVERIFY(window.findChild<QWidget *>(QStringLiteral("adminFaultPage")));
        auto *port = window.findChild<QSpinBox *>(QStringLiteral("serverPort"));
        auto *connectButton = window.findChild<QPushButton *>(QStringLiteral("connectButton"));
        auto *loginButton = window.findChild<QPushButton *>(QStringLiteral("loginButton"));
        auto *password = window.findChild<QLineEdit *>(QStringLiteral("adminPassword"));
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("adminStack"));
        auto *userCount = window.findChild<QLabel *>(QStringLiteral("dashboardUserCount"));
        QVERIFY(port && connectButton && loginButton && password && stack && userCount);
        port->setValue(server.serverPort());
        password->setText(QStringLiteral("123456"));
        QTest::mouseClick(connectButton, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(loginButton->isEnabled(), 3000);
        QTest::mouseClick(loginButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(stack->currentIndex(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(userCount->text() != QStringLiteral("--"), 3000);
        QVERIFY(window.grab().save(QDir(artifactDirectory).filePath(
            QStringLiteral("admin-client-acceptance.png"))));
        window.close();
    }
}

QTEST_MAIN(UiAcceptanceTest)
#include "ui_acceptance_test.moc"
