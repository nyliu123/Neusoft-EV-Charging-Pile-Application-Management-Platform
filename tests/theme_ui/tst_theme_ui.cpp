#include "admin_login_dialog.h"
#include "admin_main_window.h"
#include "admin_dashboard_page.h"
#include "phone_login_widget.h"
#include "station_search_widget.h"
#include "user_home_widget.h"
#include "user_api_client.h"
#include "user_session_state.h"
#include "client_ui/client_style.h"
#include "client_ui/rhine_widgets.h"
#include "common/protocol.h"
#include "network/frame_codec.h"
#include "network/platform_client.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

// A deterministic protocol fixture. No map network, real accounts, or production data.
class ThemeFixture final : public QObject {
public:
    QTcpServer server;
    QStringList requests;
    QJsonArray stations;
    QJsonObject user {{"user_id", 1}, {"nickname", QStringLiteral("测试用户")},
                      {"balance", 128.50}, {"session_id", "theme-user"}};
    ThemeFixture()
    {
        stations = {
            QJsonObject{{"station_id", 1}, {"station_name", QStringLiteral("软件园能源站")},
                {"address", QStringLiteral("大连市高新区 · 软件园路 8 号")}, {"distance_km", 1.28},
                {"price_per_kwh", 1.20}, {"idle_count", 8}, {"total_piles", 12}, {"pile_count", 12},
                {"longitude", 121.54}, {"latitude", 38.88}},
            QJsonObject{{"station_id", 2}, {"station_name", QStringLiteral("星海广场充电站")},
                {"address", QStringLiteral("大连市沙河口区 · 星海广场东侧停车场")}, {"distance_km", 2.46},
                {"price_per_kwh", 1.35}, {"idle_count", 4}, {"total_piles", 10}, {"pile_count", 10}},
            QJsonObject{{"station_id", 3}, {"station_name", QStringLiteral("滨海研究园区站")},
                {"address", QStringLiteral("大连市高新区 · 滨海路 128 号")}, {"distance_km", 3.72},
                {"price_per_kwh", 1.10}, {"idle_count", 0}, {"total_piles", 6}, {"pile_count", 6}}
        };
        connect(&server, &QTcpServer::newConnection, this, [this] {
            auto *socket = server.nextPendingConnection();
            auto buffer = QSharedPointer<QByteArray>::create();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                while (!buffer->isEmpty()) {
                    const auto decoded = ev::FrameCodec::decodeOne(*buffer);
                    if (decoded.status != ev::DecodeStatus::Complete) break;
                    respond(socket, decoded.frame.messageType, decoded.frame.payload);
                }
            });
        });
    }
    void respond(QTcpSocket *socket, quint32 type, const QJsonObject &payload)
    {
        using ev::MessageType;
        const auto data = payload.value("data").toObject();
        const QString query = data.value("type").toString();
        requests.append(query);
        quint32 responseType = type + 1;
        QJsonObject responseData, result;
        if (type == quint32(MessageType::LoginRequest)) {
            responseData = data.value("role").toString() == "admin"
                ? QJsonObject{{"admin_id", 1}, {"username", "admin"}, {"session_id", "theme-admin"}}
                : QJsonObject{{"user_info", user}, {"is_new_user", false}};
        } else if (type == quint32(MessageType::AdminQuery)) {
            responseType = quint32(MessageType::AdminResponse);
        }
        const QJsonObject pile {{"pile_id", 1}, {"pile_number", "EV-A01"},
            {"pile_type", "fast"}, {"power_kw", 60.0}, {"status", "idle"}};
        if (query == "geocode") result = {{"longitude", 121.53}, {"latitude", 38.87}};
        else if (query == "station_list") result = {{"stations", stations}};
        else if (query == "station_detail") result = {{"station", stations.first()}, {"piles", QJsonArray{pile}}};
        else if (query == "pile_list") result = {{"piles", QJsonArray{pile}}};
        else if (query == "user_info") result = user;
        else if (query == "check_pending") result = {{"has_pending", false}};
        else if (query == "reserve") result = {{"order_id", 42}};
        else if (query == "start_charge") result = {{"start_time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")},
            {"power_kw", 60.0}, {"price_per_kwh", 1.2}};
        else if (query == "end_charge") result = {{"settled", true}, {"total_kwh", 2.5},
            {"total_fee_cent", 300}, {"balance_cent", 12550}};
        else if (query == "check_pile") result = {{"available", true}, {"pile", pile},
            {"station_name", QStringLiteral("软件园能源站")}, {"price_per_kwh", 1.2}};
        else if (query == "dashboard_overview") {
            QJsonArray points;
            for (int i = 0; i < 7; ++i) points.append(QJsonObject{
                {"date", QStringLiteral("2026-09-%1").arg(i + 1, 2, 10, QLatin1Char('0'))},
                {"revenue", QJsonArray{480.0, 720.0, 590.0, 960.0, 880.0, 1100.0, 1320.0}.at(i)}});
            result = {{"summary", QJsonObject{{"total_kwh", 18426.5}, {"total_revenue", 23580.0},
                         {"total_users", 328}, {"total_orders", 1462}}},
                {"trend", QJsonObject{{"points", points}}},
                {"stats", QJsonObject{{"stats", QJsonArray{
                    QJsonObject{{"status", "idle"}, {"label", QStringLiteral("空闲")}, {"count", 12}},
                    QJsonObject{{"status", "in_use"}, {"label", QStringLiteral("使用中")}, {"count", 10}},
                    QJsonObject{{"status", "reserved"}, {"label", QStringLiteral("已预约")}, {"count", 4}},
                    QJsonObject{{"status", "fault"}, {"label", QStringLiteral("故障")}, {"count", 2}}}}}}};
        }
        if (type != quint32(MessageType::LoginRequest)) responseData = {{"result", result}};
        socket->write(ev::FrameCodec::encode(responseType, {{"success", true}, {"code", "OK"},
            {"request_id", payload.value("request_id")}, {"data", responseData}}));
    }
};

class ThemeUiTests final : public QObject {
    Q_OBJECT
private:
    void capture(QWidget &widget, const QString &name)
    {
        const QString directory = qEnvironmentVariable("EV_UI_CAPTURE_DIR");
        if (directory.isEmpty()) return;
        QVERIFY(QDir().mkpath(directory));
        QTest::qWait(80);
        QVERIFY(widget.grab().save(directory + '/' + name + ".png"));
    }
    static QPushButton *button(QWidget &parent, const QString &text)
    {
        for (auto *item : parent.findChildren<QPushButton *>()) if (item->text() == text) return item;
        return nullptr;
    }
private slots:
    void initTestCase()
    {
        ev::installClientStyle(*qApp);
        QFile file(QStringLiteral(":/styles/client.qss"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    }
    void userLoginAndStationNavigation()
    {
        ThemeFixture fixture;
        QVERIFY(fixture.server.listen(QHostAddress::LocalHost, 0));
        ev::PlatformClient client(QStringLiteral("theme-test"));
        client.connectToServer(QStringLiteral("127.0.0.1"), fixture.server.serverPort());
        QTRY_COMPARE(client.state(), ev::PlatformClient::State::Ready);
        QWidget login;
        auto *layout = new QHBoxLayout(&login);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(new ev::RhineIdentityPanel(false, &login), 1);
        auto *area = new QWidget(&login);
        auto *form = new QVBoxLayout(area);
        form->setContentsMargins(30, 28, 30, 28);
        auto *phone = new PhoneLoginWidget(area);
        form->addWidget(phone, 0, Qt::AlignCenter);
        layout->addWidget(area, 1);
        login.resize(1000, 640);
        login.show();
        capture(login, QStringLiteral("user-login"));
        QSignalSpy loginSpy(&client, &ev::PlatformClient::loginSucceeded);
        connect(phone, &PhoneLoginWidget::phoneAccepted, &client, &ev::PlatformClient::login);
        auto *input = phone->findChild<QLineEdit *>("userPhoneInput");
        QVERIFY(input);
        QTest::keyClicks(input, "13800138000");
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_COMPARE(loginSpy.count(), 1);
        QVERIFY(UserSessionState::instance().setUserInfo(loginSpy.first().first().toJsonObject()));
        login.hide();
        ev::UserApiClient api(&client);
        UserHomeWidget home(&api);
        home.resize(1200, 780);
        home.refresh();
        home.show();
        auto *stationPage = home.findChild<StationSearchWidget *>();
        QVERIFY(stationPage);
        auto *address = stationPage->findChild<QLineEdit *>();
        QVERIFY(address);
        address->setText(QStringLiteral("大连软件园"));
        auto *search = button(*stationPage, QStringLiteral("搜索"));
        QVERIFY(search);
        QTest::mouseClick(search, Qt::LeftButton);
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("station_list")));
        QTRY_VERIFY(search->isEnabled());
        QPushButton *stationCard = nullptr;
        for (auto *candidate : stationPage->findChildren<QPushButton *>())
            if (candidate->property("uiClass").toString() == "stationCard") { stationCard = candidate; break; }
        QVERIFY(stationCard);
        QVERIFY(stationCard->accessibleName().contains(QStringLiteral("软件园能源站")));
        capture(home, QStringLiteral("user-stations"));
        // Verify mouse clicks on the visual card (including over child labels) reach its action.
        QTest::mouseClick(stationCard, Qt::LeftButton, Qt::NoModifier, QPoint(150, 30));
        auto *pileTable = stationPage->findChild<QTableWidget *>();
        QVERIFY(pileTable);
        QTRY_COMPARE(pileTable->rowCount(), 1);
        capture(home, QStringLiteral("user-station-detail"));
        auto *choose = button(*stationPage, QStringLiteral("选择"));
        QVERIFY(choose);
        QTest::mouseClick(choose, Qt::LeftButton);
        auto *tabs = home.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QTRY_COMPARE(tabs->currentIndex(), 1);
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("check_pile")));
        capture(home, QStringLiteral("user-charge"));
        const auto activeStep = [&home]() {
            for (auto *label : home.findChildren<QLabel *>()) {
                if (label->property("uiClass").toString() == "flowStep" && label->property("active").toBool())
                    return label->text().left(2);
            }
            return QString();
        };
        QCOMPARE(activeStep(), QStringLiteral("01"));
        QTimer confirm;
        connect(&confirm, &QTimer::timeout, &home, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                box->button(box->standardButtons().testFlag(QMessageBox::Yes)
                    ? QMessageBox::Yes : QMessageBox::Ok)->click();
            }
        });
        confirm.start(20);
        auto *reserve = button(home, QStringLiteral("预约充电"));
        QVERIFY(reserve);
        QTest::mouseClick(reserve, Qt::LeftButton);
        QTRY_COMPARE(activeStep(), QStringLiteral("02"));
        auto *start = button(home, QStringLiteral("开始充电"));
        QVERIFY(start && start->isVisible());
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_COMPARE(activeStep(), QStringLiteral("03"));
        QVERIFY(home.findChild<QProgressBar *>()->isVisible());
        capture(home, QStringLiteral("user-charging"));
        auto *end = button(home, QStringLiteral("结束充电"));
        QVERIFY(end && end->isVisible());
        QTest::mouseClick(end, Qt::LeftButton);
        QTRY_COMPARE(activeStep(), QStringLiteral("04"));
        capture(home, QStringLiteral("user-settled"));
        confirm.stop();
        tabs->setCurrentIndex(2);
        capture(home, QStringLiteral("user-profile"));
        home.resize(1000, 680);
        tabs->setCurrentIndex(0);
        capture(home, QStringLiteral("user-small"));
        UserSessionState::instance().clear();
    }
    void adminLoginAndNavigation()
    {
        ThemeFixture fixture;
        QVERIFY(fixture.server.listen(QHostAddress::LocalHost, 0));
        ev::PlatformClient client(QStringLiteral("theme-admin-test"));
        client.connectToServer(QStringLiteral("127.0.0.1"), fixture.server.serverPort());
        QTRY_COMPARE(client.state(), ev::PlatformClient::State::Ready);
        ev::AdminLoginDialog login(&client);
        login.show();
        capture(login, QStringLiteral("admin-login"));
        auto *username = login.findChild<QLineEdit *>("adminUsernameInput");
        auto *password = login.findChild<QLineEdit *>("adminPasswordInput");
        auto *submit = login.findChild<QPushButton *>("adminLoginButton");
        QVERIFY(username && password && submit);
        QTest::mouseClick(submit, Qt::LeftButton);
        QVERIFY(!login.session().isLoggedIn);
        username->setText(QStringLiteral("admin"));
        password->setText(QStringLiteral("fixture-password"));
        auto *toggle = button(login, QStringLiteral("显示"));
        QVERIFY(toggle);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(password->echoMode(), QLineEdit::Normal);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(password->echoMode(), QLineEdit::Password);
        QSignalSpy accepted(&login, &QDialog::accepted);
        QTest::mouseClick(submit, Qt::LeftButton);
        QTRY_COMPARE(accepted.count(), 1);
        ev::AdminMainWindow main(&client, login.session());
        main.show();
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("dashboard_overview")));
        QTest::qWait(100);
        capture(main, QStringLiteral("admin-dashboard"));
        auto *nav = main.findChild<QListWidget *>("sidebarNavigation");
        QVERIFY(nav);
        QCOMPARE(nav->count(), 5);
        nav->setCurrentRow(2);
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("station_list")));
        QTest::qWait(100);
        capture(main, QStringLiteral("admin-stations"));
        for (int row : {1, 3, 4}) { nav->setCurrentRow(row); QTest::qWait(80); }
        capture(main, QStringLiteral("admin-orders"));
        nav->setCurrentRow(0);
        main.resize(1100, 680);
        capture(main, QStringLiteral("admin-small"));
    }
};

QTEST_MAIN(ThemeUiTests)
#include "tst_theme_ui.moc"
