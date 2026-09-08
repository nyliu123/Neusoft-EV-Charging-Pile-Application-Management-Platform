#include "admin_login_dialog.h"
#include "admin_main_window.h"
#include "admin_dashboard_page.h"
#include "admin_membership_page.h"
#include "membership_dialog.h"
#include "phone_login_widget.h"
#include "station_search_widget.h"
#include "user_home_widget.h"
#include "user_api_client.h"
#include "user_session_state.h"
#include "client_ui/client_style.h"
#include "client_ui/apple_widgets.h"
#include "common/protocol.h"
#include "network/frame_codec.h"
#include "network/platform_client.h"
#include <QDir>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QLineEdit>
#include <QLabel>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

// A deterministic protocol fixture. No map network, real accounts, or production data.
class ThemeFixture final : public QObject {
public:
    QTcpServer server;
    QStringList requests;
    QJsonArray stations;
    bool liked = false;
    bool posted = false;
    QString postedContent;
    int postedRating = 0;
    QJsonArray plans, articles;
    QJsonObject memberState{{"valid",false},{"level","NORMAL"},{"discount_bps",10000}};
    QHash<QString,QJsonObject> lastParams;
    bool consultFailure=false;
    QJsonObject user {{"user_id", 1}, {"nickname", QStringLiteral("测试用户")},
                      {"balance", 128.50}, {"session_id", "theme-user"}};
    ThemeFixture()
    {
        int id=0;
        for(const auto &level:QStringList{"VIP","SVIP"})for(int recurring:{0,1})for(int months:{1,3,12}){
            plans.append(QJsonObject{{"plan_id",++id},{"version",1},{"level",level},{"months",months},{"recurring",recurring},
                {"price_cent",months*(level=="VIP"?(recurring?1200:1500):(recurring?2500:3000))},{"discount_bps",level=="VIP"?9000:8000},{"active",1}});
        }
        articles.append(QJsonObject{{"article_id",1},{"draft_version",1},{"published_version",1},{"active",1},
            {"title",QStringLiteral("充电操作")},{"keywords",QStringLiteral("充电,预约,开始")},
            {"content",QStringLiteral("先预约空闲充电桩，再开始模拟充电。结束后从模拟钱包结算。")},{"source",QStringLiteral("项目教学说明：充电流程")}});
        stations = {
            QJsonObject{{"station_id", 1}, {"station_name", QStringLiteral("软件园能源站")},
                {"address", QStringLiteral("大连市高新区 · 软件园路 8 号")}, {"distance_km", 1.28},
                {"price_per_kwh", 1.20}, {"idle_count", 8}, {"total_piles", 12}, {"pile_count", 12},
                {"longitude", 121.54}, {"latitude", 38.88},
                {"rating", QJsonObject{{"count", 1}, {"avg", 10.0}, {"tier", QStringLiteral("优秀")},
                    {"hot", QJsonObject{{"content", QStringLiteral("车位宽敞，充电体验很好。")}}}}}},
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
        const auto params = data.value("params").toObject();
        lastParams.insert(query,params);
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
        if(query=="plans" || query=="membership_plans")result={{"plans",plans}};
        else if(query=="status")result=memberState;
        else if(query=="purchase"){
            const auto p=plans[params.value("plan_id").toInt()-1].toObject();
            const auto now=QDateTime::currentSecsSinceEpoch();
            memberState={{"valid",true},{"level",p.value("level")},{"discount_bps",p.value("discount_bps")},
                {"starts_at",now},{"expires_at",now+30*86400},{"paid_until",now+30*86400},
                {"renewal",p.value("recurring").toInt()?QJsonObject{{"status","active"},{"next_due",now+30*86400},{"price_cent",p.value("price_cent")}}:QJsonObject{}}};
            result={{"paid_cent",p.value("price_cent")},{"balance_cent",97500},{"expires_at",now+30*86400}};
        }
        else if(query=="set_renewal"){
            auto renewal=memberState.value("renewal").toObject();renewal.insert("status",params.value("enabled").toBool()?"active":"cancelled");memberState.insert("renewal",renewal);
        }
        else if(query=="ask")result={{"answer",QStringLiteral("请先预约空闲充电桩，再开始模拟充电。[1]")},{"generated",true},
            {"sources",QJsonArray{QJsonObject{{"article_id",1},{"title",QStringLiteral("充电操作")},{"version",1},{"source",QStringLiteral("项目教学说明：充电流程")}}}}};
        else if(query=="membership_update"){
            for(int i=0;i<plans.size();++i)if(plans[i].toObject().value("plan_id")==params.value("plan_id")){
                auto p=plans[i].toObject();p.insert("version",p.value("version").toInt()+1);p.insert("price_cent",params.value("price_cent"));plans[i]=p;break;
            }
        }
        else if(query=="knowledge_list")result={{"articles",articles}};
        else if(query=="knowledge_save"){
            auto p=params;p.insert("draft_version",params.value("draft_version").toInt()+1);p.insert("active",1);p.insert("published_version",1);articles[0]=p;result={{"article_id",1}};
        }
        else if(query=="knowledge_publish" || query=="knowledge_disable"){
            auto p=articles[0].toObject();p.insert("published_version",p.value("draft_version"));p.insert("active",query=="knowledge_publish"?1:0);articles[0]=p;
        }
        else if (query == "geocode") result = {{"longitude", 121.53}, {"latitude", 38.87}};
        else if (query == "station_list") result = {{"stations", stations}};
        else if (query == "station_detail") result = {{"station", stations.first()}, {"piles", QJsonArray{pile}}};
        else if (query == "pile_list") result = {{"piles", QJsonArray{pile}}};
        else if (query == "list_comments") {
            QJsonArray comments {QJsonObject{{"comment_id", 7}, {"nickname", QStringLiteral("周同学")},
                {"content", QStringLiteral("车位宽敞，充电体验很好。")}, {"rating", 5},
                {"created_at", "2026-09-08 09:30"}, {"is_mine", false},
                {"like_count", liked ? 3 : 2}, {"liked_by_me", liked}}};
            if (posted) comments.append(QJsonObject{{"comment_id", 8},
                {"nickname", QStringLiteral("测试用户")}, {"content", postedContent},
                {"rating", postedRating}, {"created_at", "2026-09-08 10:00"},
                {"is_mine", true}, {"like_count", 0}, {"liked_by_me", false}});
            result = {{"comments", comments}, {"summary", QJsonObject{
                {"avg", posted ? 9.0 : 10.0}, {"tier", QStringLiteral("优秀")}, {"count", posted ? 2 : 1}}}};
        } else if (query == "toggle_like") {
            liked = !liked;
            result = {{"liked", liked}};
        } else if (query == "post_comment") {
            const auto params = data.value("params").toObject();
            postedContent = params.value("content").toString();
            postedRating = params.value("rating").toInt();
            result = {{"updated", posted}};
            posted = true;
        }
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
        const bool fail=consultFailure && query=="ask";
        if(fail)responseData={{"result",QJsonObject{}}};
        socket->write(ev::FrameCodec::encode(responseType, {{"success", !fail}, {"code", fail?"MODEL_UNAVAILABLE":"OK"},
            {"message",fail?QStringLiteral("AI服务尚未配置，请管理员设置服务端 EV_AI_API_KEY"):QString()},
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
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(0);
        layout->addWidget(new ev::AppleIdentityPanel(false, &login), 1);
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
        auto *grid = home.findChild<QWidget *>("stationList");
        QVERIFY(grid);
        QTRY_COMPARE(grid->property("columns").toInt(), 3);
        capture(home, QStringLiteral("user-stations"));
        home.resize(900, 600);
        QTRY_COMPARE(grid->property("columns").toInt(), 2);
        QCOMPARE(home.size(), QSize(900, 600));
        QVERIFY(stationCard->width() >= 260);
        QTRY_VERIFY(stationCard->parentWidget()->parentWidget()->height() >= stationCard->height());
        capture(home, QStringLiteral("user-discovery-small"));
        home.resize(1200, 780);
        QTRY_COMPARE(grid->property("columns").toInt(), 3);
        // Verify mouse clicks on the visual card (including over child labels) reach its action.
        QTest::mouseClick(stationCard, Qt::LeftButton, Qt::NoModifier, QPoint(150, 30));
        auto *pileTable = stationPage->findChild<QTableWidget *>();
        QVERIFY(pileTable);
        QTRY_COMPARE(pileTable->rowCount(), 1);
        capture(home, QStringLiteral("user-station-detail"));
        auto *comments = button(*stationPage, QStringLiteral("查看评论（1）"));
        QVERIFY(comments && comments->isVisible());
        QTest::mouseClick(comments, Qt::LeftButton);
        QTRY_VERIFY(button(*stationPage, QStringLiteral("赞 2")));
        QTest::mouseClick(button(*stationPage, QStringLiteral("赞 2")), Qt::LeftButton);
        QTRY_VERIFY(fixture.liked);
        QTRY_VERIFY(button(*stationPage, QStringLiteral("已赞 3")));
        auto *commentInput = stationPage->findChild<QPlainTextEdit *>();
        auto *fourStars = stationPage->findChild<QPushButton *>("ratingStar4");
        QVERIFY(commentInput && fourStars);
        commentInput->setPlainText(QStringLiteral("位置好找，操作很方便。"));
        QTest::mouseClick(fourStars, Qt::LeftButton);
        QTimer commentConfirm;
        connect(&commentConfirm, &QTimer::timeout, &home, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Ok)->click();
        });
        commentConfirm.start(20);
        QTest::mouseClick(button(*stationPage, QStringLiteral("发表评论")), Qt::LeftButton);
        QTRY_VERIFY(fixture.posted);
        QTRY_VERIFY(button(*stationPage, QStringLiteral("修改我的评论")));
        QCOMPARE(fixture.postedRating, 4);
        QCOMPARE(fixture.postedContent, QStringLiteral("位置好找，操作很方便。"));
        commentConfirm.stop();
        capture(home, QStringLiteral("user-comments"));
        QTest::mouseClick(button(*stationPage, QStringLiteral("< 返回站点详情")), Qt::LeftButton);
        auto *choose = button(*stationPage, QStringLiteral("选择"));
        QVERIFY(choose);
        QTest::mouseClick(choose, Qt::LeftButton);
        auto *tabs = home.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QTRY_COMPARE(tabs->currentIndex(), 1);
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("check_pile")));
        auto *navigation = home.findChild<QListWidget *>("userNavigation");
        QVERIFY(navigation);
        QCOMPARE(navigation->count(), 4);
        QCOMPARE(navigation->currentRow(), 1);
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
        // Mouse and keyboard navigation must update the existing page state.
        QTest::mouseClick(navigation->viewport(), Qt::LeftButton, Qt::NoModifier,
                          navigation->visualItemRect(navigation->item(2)).center());
        QCOMPARE(tabs->currentIndex(), 2);
        auto *avatar = home.findChild<QLabel *>("profileAvatar");
        QVERIFY(avatar && !avatar->pixmap().isNull());
        capture(home, QStringLiteral("user-profile"));
        navigation->setFocus();
        QTest::keyClick(navigation, Qt::Key_Right);
        QCOMPARE(tabs->currentIndex(), 3);
        tabs->setCurrentIndex(0);
        QCOMPARE(navigation->currentRow(), 0);
        home.resize(900, 600);
        tabs->setCurrentIndex(0);
        capture(home, QStringLiteral("user-small"));
        QCOMPARE(home.size(), QSize(900, 600));
        UserSessionState::instance().clear();
    }
    void stationGridReflowsWithoutLosingActions()
    {
        ev::AdaptiveStationGrid grid;
        grid.resize(1120, 500);
        QVector<QPushButton *> cards;
        for (int i = 0; i < 7; ++i) {
            auto *card = new ev::AppleStationCard(QStringLiteral("充电站 %1").arg(i),
                QStringLiteral("测试地址"), QStringLiteral("1.2 km"),
                QStringLiteral("¥1.20/度"), QStringLiteral("空闲 2 / 6"),
                QStringLiteral("评分 9.0"), true, &grid);
            cards.append(card);
            grid.addCard(card);
        }
        grid.show();
        QTRY_COMPARE(grid.property("columns").toInt(), 3);
        auto *layout = qobject_cast<QGridLayout *>(grid.layout());
        QVERIFY(layout);
        QCOMPARE(layout->itemAtPosition(2, 0)->widget(), cards.last());
        QSignalSpy clicked(cards.first(), &QPushButton::clicked);
        grid.activateWindow();
        cards.first()->setFocus();
        QTRY_VERIFY(cards.first()->hasFocus());
        for (const auto &size : {QSize(800, 500), QSize(600, 500), QSize(1120, 500)}) {
            grid.resize(size);
            const int columns = size.width() >= 1050 ? 3 : size.width() >= 680 ? 2 : 1;
            QTRY_COMPARE(grid.property("columns").toInt(), columns);
            QCOMPARE(layout->itemAtPosition(6 / columns, 6 % columns)->widget(), cards.last());
            QVERIFY(cards.first()->hasFocus());
            QTest::keyClick(cards.first(), Qt::Key_Space);
        }
        QCOMPARE(clicked.count(), 3);
        grid.clear();
        QCOMPARE(layout->count(), 0);
        QCOMPARE(grid.minimumHeight(), 0);
        QVERIFY(grid.findChildren<QPushButton *>().isEmpty());
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
        QCOMPARE(nav->count(), 7);
        nav->setCurrentRow(2);
        QTRY_VERIFY(fixture.requests.contains(QStringLiteral("station_list")));
        QTest::qWait(100);
        capture(main, QStringLiteral("admin-stations"));
        for (int row : {1, 3, 4}) { nav->setCurrentRow(row); QTest::qWait(80); }
        capture(main, QStringLiteral("admin-orders"));
        nav->setCurrentRow(5);
        auto *plans=main.findChild<QTableWidget *>("adminMembershipPlans");QVERIFY(plans);
        QTRY_COMPARE(plans->rowCount(),12);
        capture(main,QStringLiteral("admin-membership"));
        auto *price=qobject_cast<QDoubleSpinBox *>(plans->cellWidget(0,2));QVERIFY(price);price->setValue(17.50);
        QTimer::singleShot(80,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});
        QTest::mouseClick(main.findChild<QPushButton *>("savePlan1"),Qt::LeftButton);
        QTRY_COMPARE(fixture.lastParams.value("membership_update").value("price_cent").toInt(),1750);
        QTRY_VERIFY(plans->isEnabled());
        nav->setCurrentRow(6);
        auto *articles=main.findChild<QListWidget *>("knowledgeArticles");QVERIFY(articles);
        QTRY_COMPARE(articles->count(),1);
        capture(main,QStringLiteral("admin-knowledge"));
        main.findChild<QLineEdit *>("knowledgeTitle")->setText(QStringLiteral("更新后的充电操作"));
        QTest::mouseClick(main.findChild<QPushButton *>("knowledge_publish"),Qt::LeftButton);
        QVERIFY(!fixture.requests.contains("knowledge_publish")); // Unsaved changes cannot publish.
        QTest::mouseClick(main.findChild<QPushButton *>("knowledge_save"),Qt::LeftButton);
        QTRY_VERIFY(fixture.requests.contains("knowledge_save"));
        QTRY_COMPARE(fixture.articles[0].toObject().value("draft_version").toInt(),2);
        QTRY_VERIFY(main.findChild<QPushButton *>("knowledge_publish")->isEnabled());
        QTest::qWait(80);
        QTimer::singleShot(80,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});
        QTest::mouseClick(main.findChild<QPushButton *>("knowledge_publish"),Qt::LeftButton);
        QTRY_VERIFY(fixture.requests.contains("knowledge_publish"));
        nav->setCurrentRow(0);
        main.resize(1100, 680);
        capture(main, QStringLiteral("admin-small"));
    }
    void membershipPurchaseConsentCancellationAndConsult()
    {
        ThemeFixture fixture;QVERIFY(fixture.server.listen(QHostAddress::LocalHost,0));
        ev::PlatformClient client("member-ui-test");client.connectToServer("127.0.0.1",fixture.server.serverPort());
        QTRY_COMPARE(client.state(),ev::PlatformClient::State::Ready);
        QVERIFY(UserSessionState::instance().setUserInfo(fixture.user));QSettings().clear();
        ev::UserApiClient api(&client);
        MembershipDialog dialog(&api);dialog.setAttribute(Qt::WA_DeleteOnClose,false);dialog.show();
        QTRY_VERIFY(dialog.findChild<QPushButton *>("buyPlan7"));
        QCOMPARE(dialog.findChildren<QPushButton *>(QRegularExpression("^buyPlan")).size(),6);
        capture(dialog,QStringLiteral("membership-fixed"));
        bool fixedNoConsent=false;
        QTimer::singleShot(80,[&]{
            auto *modal=QApplication::activeModalWidget();
            auto *check=modal->findChild<QCheckBox *>("renewalConsent");auto *box=modal->findChild<QDialogButtonBox *>();
            fixedNoConsent=!check->isVisible() && box->button(QDialogButtonBox::Ok)->isEnabled();
            QTest::mouseClick(box->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        QTest::mouseClick(dialog.findChild<QPushButton *>("buyPlan7"),Qt::LeftButton);
        QVERIFY(fixedNoConsent);QTRY_COMPARE(fixture.requests.count("purchase"),1);
        QTRY_VERIFY(dialog.findChild<QComboBox *>("membershipBilling")->isEnabled());
        QVERIFY(!fixture.lastParams.value("purchase").value("renewal_consent").toBool());
        auto *billing=dialog.findChild<QComboBox *>("membershipBilling");billing->setCurrentIndex(1);
        QTRY_VERIFY(dialog.findChild<QPushButton *>("buyPlan10"));
        QCOMPARE(dialog.findChildren<QPushButton *>(QRegularExpression("^buyPlan")).size(),6);
        capture(dialog,QStringLiteral("membership-recurring"));
        bool explicitConsent=false;
        QTimer::singleShot(80,[&]{
            auto *modal=QApplication::activeModalWidget();auto *check=modal->findChild<QCheckBox *>("renewalConsent");auto *box=modal->findChild<QDialogButtonBox *>();
            explicitConsent=check->isVisible() && !check->isChecked() && !box->button(QDialogButtonBox::Ok)->isEnabled();
            capture(*modal,QStringLiteral("membership-consent"));check->setChecked(true);
            QTest::mouseClick(box->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        QTest::mouseClick(dialog.findChild<QPushButton *>("buyPlan10"),Qt::LeftButton);
        QVERIFY(explicitConsent);QTRY_COMPARE(fixture.requests.count("purchase"),2);
        QTRY_VERIFY(billing->isEnabled());QVERIFY(fixture.lastParams.value("purchase").value("renewal_consent").toBool());
        QTimer::singleShot(80,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});
        QTest::mouseClick(dialog.findChild<QPushButton *>("membershipCancelRenewal"),Qt::LeftButton);
        QTRY_COMPARE(fixture.memberState.value("renewal").toObject().value("status").toString(),QString("cancelled"));
        QTRY_VERIFY(dialog.findChild<QLabel *>("renewalStatus")->text().contains(QStringLiteral("已取消")));
        dialog.findChild<QTabWidget *>()->setCurrentIndex(1);
        auto *question=dialog.findChild<QPlainTextEdit *>("consultQuestion");auto *chat=dialog.findChild<QPlainTextEdit *>("consultTranscript");auto *send=dialog.findChild<QPushButton *>("consultSend");
        question->setPlainText(QStringLiteral("如何预约充电？"));QTest::mouseClick(send,Qt::LeftButton);
        QTRY_VERIFY(chat->toPlainText().contains(QStringLiteral("版本1")));
        capture(dialog,QStringLiteral("membership-consult"));
        fixture.consultFailure=true;question->setPlainText(QStringLiteral("会员怎样续费？"));QTest::mouseClick(send,Qt::LeftButton);
        QTRY_VERIFY(chat->toPlainText().contains("EV_AI_API_KEY"));
        capture(dialog,QStringLiteral("membership-consult-no-key"));
        QVERIFY(!QSettings().contains("membership/pending/1"));
    }
    void membershipRetriesOriginalOperationAfterReopening()
    {
        ThemeFixture fixture;QVERIFY(fixture.server.listen(QHostAddress::LocalHost,0));
        ev::PlatformClient client("member-retry-test");client.connectToServer("127.0.0.1",fixture.server.serverPort());
        QTRY_COMPARE(client.state(),ev::PlatformClient::State::Ready);
        QVERIFY(UserSessionState::instance().setUserInfo(fixture.user));
        const QJsonObject original{{"operation_key","unconfirmed-original-operation"},{"plan_id",7},{"version",1},{"renewal_consent",false}};
        QSettings().setValue("membership/pending/1",QJsonDocument(original).toJson(QJsonDocument::Compact));
        ev::UserApiClient api(&client);MembershipDialog dialog(&api);dialog.setAttribute(Qt::WA_DeleteOnClose,false);dialog.show();
        auto *retry=dialog.findChild<QPushButton *>("membershipRetry");QVERIFY(retry && retry->isVisible());
        QTest::mouseClick(retry,Qt::LeftButton);
        QTRY_COMPARE(fixture.lastParams.value("purchase"),original);
        QTRY_VERIFY(!QSettings().contains("membership/pending/1"));
    }
};

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("EV Isolated UI Tests");
    QCoreApplication::setApplicationName("Membership regression");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    ThemeUiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "tst_theme_ui.moc"
