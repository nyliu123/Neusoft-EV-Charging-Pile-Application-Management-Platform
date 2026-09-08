#include "order_list_widget.h"
#include "navigation_map_dialog.h"
#include "client_ui/animated_combo_box.h"
#include "client_ui/client_style.h"
#include <QWebEngineView>
#include <QComboBox>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QPointer>
#include <limits>
#include "user_home_widget.h"
#include "charge_flow_widget.h"
#include <QTabWidget>
#include "user_api_client.h"
#include "user_session_state.h"
#include "network/platform_client.h"
#include "network/frame_codec.h"
#include "common/protocol.h"

#include <QLabel>
#include <QFile>
#include <QEventLoop>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class OrderUiTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void listDetailsRetryAndStaleResponses();
    void embeddedMapLoadsSwitchesModeAndHandlesFailures();
    void realBeijingRouteService();
};

void OrderUiTests::initTestCase()
{
    ev::installClientStyle(*qApp);
    QFile stylesheet(qEnvironmentVariable("EV_ORDER_TEST_STYLESHEET",
                                           QStringLiteral(":/styles/client.qss")));
    QVERIFY(stylesheet.open(QIODevice::ReadOnly));
    qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
}

void OrderUiTests::listDetailsRetryAndStaleResponses()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    ev::PlatformClient client(QStringLiteral("order-ui-test"));
    client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    QTRY_VERIFY(server.hasPendingConnections());
    auto *socket = server.nextPendingConnection();
    QByteArray buffer;
    QStringList requests;
    const auto readIncoming = [&] {
        buffer += socket->readAll();
        while (!buffer.isEmpty()) {
            const auto decoded = ev::FrameCodec::decodeOne(buffer);
            if (decoded.status != ev::DecodeStatus::Complete) {
                break;
            }
            if (decoded.frame.messageType == quint32(ev::MessageType::HealthRequest)) {
                socket->write(ev::FrameCodec::encode(quint32(ev::MessageType::HealthResponse),
                    {{QStringLiteral("success"), true}}));
            } else {
                QCOMPARE(decoded.frame.payload.value(QStringLiteral("data")).toObject()
                             .value(QStringLiteral("type")).toString(), QStringLiteral("query_orders"));
                requests.append(decoded.frame.payload.value(QStringLiteral("request_id")).toString());
            }
        }
    };
    connect(socket, &QTcpSocket::readyRead, this, readIncoming);
    readIncoming();
    QTRY_COMPARE(client.state(), ev::PlatformClient::State::Ready);
    QVERIFY(UserSessionState::instance().setUserInfo({
        {QStringLiteral("user_id"), 1}, {QStringLiteral("session_id"), QStringLiteral("test-session")}}));
    ev::UserApiClient api(&client);
    UserHomeWidget home(&api);
    auto *orderPage = home.findChild<OrderListWidget *>();
    auto *chargePage = home.findChild<ChargeFlowWidget *>();
    auto *tabs = home.findChild<QTabWidget *>();
    QVERIFY(orderPage && chargePage && tabs);
    QCOMPARE(tabs->count(), 4);
    QCOMPARE(tabs->tabText(tabs->indexOf(chargePage)), QStringLiteral("充电"));
    QCOMPARE(tabs->tabText(tabs->indexOf(orderPage)), QStringLiteral("我的订单"));
    auto &widget = *orderPage;
    home.resize(1000, 680);
    home.show();
    auto *status = widget.findChild<QLabel *>(QStringLiteral("ordersStatus"));
    auto *refresh = widget.findChild<QPushButton *>(QStringLiteral("refreshOrdersButton"));
    QVERIFY(status && refresh);
    const QJsonArray orders {QJsonObject {
        {QStringLiteral("order_id"), 42}, {QStringLiteral("station_name"), QStringLiteral("软件园充电站")},
        {QStringLiteral("pile_number"), QStringLiteral("A-01")},
        {QStringLiteral("status"), QStringLiteral("pending_settlement")},
        {QStringLiteral("status_text"), QStringLiteral("待结算")},
        {QStringLiteral("reserve_time"), QStringLiteral("2026-09-08 09:00:00")},
        {QStringLiteral("start_time"), QStringLiteral("2026-09-08 09:01:00")},
        {QStringLiteral("end_time"), QStringLiteral("2026-09-08 10:31:00")},
        {QStringLiteral("duration_hours"), 1.5}, {QStringLiteral("charge_amount_kwh"), 12.5},
        {QStringLiteral("price_per_kwh"), 1.2}, {QStringLiteral("total_fee"), 15.0}
    }};
    auto respond = [&](int index, bool success, const QJsonArray &data) {
        socket->write(ev::FrameCodec::encode(quint32(ev::MessageType::UserResponse), {
            {QStringLiteral("request_id"), requests[index]}, {QStringLiteral("success"), success},
            {QStringLiteral("code"), success ? QStringLiteral("OK") : QStringLiteral("STORAGE_ERROR")},
            {QStringLiteral("data"), QJsonObject {{QStringLiteral("result"),
                QJsonObject {{QStringLiteral("orders"), data}}}}}
        }));
    };
    tabs->setCurrentWidget(orderPage);
    QVERIFY(!refresh->isEnabled());
    QTRY_COMPARE(requests.size(), 1);
    respond(0, true, orders);
    QTRY_VERIFY(refresh->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral("1 笔")));
    auto *details = widget.findChild<QLabel *>(QStringLiteral("orderDetails"));
    QVERIFY(details && details->isHidden());
    for (auto *button : widget.findChildren<QPushButton *>()) {
        if (button->isCheckable()) {
            button->click();
            QVERIFY(!details->isHidden());
            QVERIFY(details->text().contains(QStringLiteral("1.50 小时")));
            const auto screenshot = qEnvironmentVariable("EV_ORDER_TEST_SCREENSHOT");
            if (!screenshot.isEmpty()) {
                QTest::qWait(50);
                QVERIFY(home.grab().save(screenshot));
            }
            button->click();
            QVERIFY(details->isHidden());
        }
    }
    widget.refresh();
    QTRY_COMPARE(requests.size(), 2);
    respond(1, false, {});
    QTRY_VERIFY(refresh->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral("失败")));
    refresh->click();
    QTRY_COMPARE(requests.size(), 3);
    respond(2, true, {});
    QTRY_COMPARE(status->text(), QStringLiteral("暂无充电订单"));
    widget.refresh();
    QTRY_COMPARE(requests.size(), 4);
    widget.reset();
    respond(3, true, orders);
    QTest::qWait(100);
    QVERIFY(widget.findChildren<QLabel *>(QStringLiteral("orderDetails")).isEmpty());
    QVERIFY(status->text() != QStringLiteral("暂无充电订单"));
    widget.refresh();
    QTRY_COMPARE(requests.size(), 5);
    QTRY_VERIFY_WITH_TIMEOUT(refresh->isEnabled(), 17000);
    QVERIFY(status->text().contains(QStringLiteral("超时")));
    respond(4, true, orders);
    QTest::qWait(100);
    QVERIFY(widget.findChildren<QLabel *>(QStringLiteral("orderDetails")).isEmpty());
    QSignalSpy back(&widget, &OrderListWidget::backRequested);
    for (auto *button : widget.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("返回首页")) {
            button->click();
        }
    }
    QCOMPARE(back.count(), 1);
    QCOMPARE(tabs->currentIndex(), 0);
    UserSessionState::instance().clear();
}


void OrderUiTests::embeddedMapLoadsSwitchesModeAndHandlesFailures()
{
    const auto driving = NavigationMapDialog::directionsUrl(121.5, 38.9, 121.6, 38.8, 0);
    const auto walking = NavigationMapDialog::directionsUrl(121.5, 38.9, 121.6, 38.8, 1);
    const auto cycling = NavigationMapDialog::directionsUrl(121.5, 38.9, 121.6, 38.8, 2);
    const auto drivingRequest = QJsonDocument::fromJson(QUrlQuery(driving)
        .queryItemValue(QStringLiteral("json")).toUtf8()).object();
    const auto walkingRequest = QJsonDocument::fromJson(QUrlQuery(walking)
        .queryItemValue(QStringLiteral("json")).toUtf8()).object();
    const auto cyclingRequest = QJsonDocument::fromJson(QUrlQuery(cycling)
        .queryItemValue(QStringLiteral("json")).toUtf8()).object();
    QCOMPARE(drivingRequest.value(QStringLiteral("costing")).toString(), QStringLiteral("auto"));
    QCOMPARE(walkingRequest.value(QStringLiteral("costing")).toString(), QStringLiteral("pedestrian"));
    QCOMPARE(cyclingRequest.value(QStringLiteral("costing")).toString(), QStringLiteral("bicycle"));
    QVERIFY(NavigationMapDialog::directionsUrl(181, 0, 0, 0, 0).isEmpty());
    QVERIFY(NavigationMapDialog::directionsUrl(0, std::numeric_limits<double>::quiet_NaN(), 0, 0, 0).isEmpty());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    qputenv("EV_NAVIGATION_ROUTE_ENDPOINT",
            QStringLiteral("http://127.0.0.1:%1/route").arg(server.serverPort()).toUtf8());
    qputenv("EV_NAVIGATION_TILE_TEMPLATE",
            QByteArray("data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs="));
    bool fail = false;
    QString lastCosting;
    int routeRequestCount = 0;
    connect(&server, &QTcpServer::newConnection, this, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer] {
                *buffer += socket->readAll();
                if (!buffer->contains("\r\n\r\n")) return;
                const auto path = buffer->split(' ').value(1);
                const auto routeRequest = QJsonDocument::fromJson(QUrlQuery(QUrl(QString::fromUtf8(path)))
                    .queryItemValue(QStringLiteral("json")).toUtf8()).object();
                lastCosting = routeRequest.value(QStringLiteral("costing")).toString();
                ++routeRequestCount;
                if (fail) {
                    socket->disconnectFromHost();
                    return;
                }
                const QByteArray body = R"JSON({"trip":{"legs":[{"shape":"czsbkAaw}~|EkgeBvc`E"}],"summary":{"length":15.2,"time":801},"status":0,"status_message":"Found route"}})JSON";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    QWidget dialogParent;
    QPointer<NavigationMapDialog> dialog = new NavigationMapDialog(
        121.5, 38.9, 121.6, 38.8, QStringLiteral("起点测试地址"),
        QStringLiteral("测试站点"), QStringLiteral("终点测试地址"), 0, &dialogParent);
    auto *view = dialog->findChild<QWebEngineView *>(QStringLiteral("embeddedMapView"));
    auto *status = dialog->findChild<QLabel *>(QStringLiteral("mapLoadStatus"));
    auto *mode = dialog->findChild<QComboBox *>(QStringLiteral("navigationMode"));
    QVERIFY(view && status && mode);
    QCOMPARE(status->alignment(), Qt::AlignCenter);
    QCOMPARE(status->property("uiClass").toString(), QStringLiteral("mapStatus"));
    QVERIFY(qobject_cast<ev::AnimatedComboBox *>(mode));
    bool hasTravelModeLabel = false;
    for (const auto *label : dialog->findChildren<QLabel *>()) {
        hasTravelModeLabel |= label->text() == QStringLiteral("出行方式：");
    }
    QVERIFY(hasTravelModeLabel);
    QVERIFY(!dialog->findChild<QPushButton *>(QStringLiteral("retryMapButton")));
    QCOMPARE(mode->count(), 3);
    QCOMPARE(mode->itemText(2), QStringLiteral("骑行"));
    dialog->show();
    QTRY_COMPARE(routeRequestCount, 1);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 10000);
    QVERIFY(status->isHidden());
    QVERIFY(status->text().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(view->title(), QStringLiteral("地图路线"), 5000);
    QCOMPARE(lastCosting, QStringLiteral("auto"));
    const auto runJavascript = [&](const QString &script) -> QVariant {
        QVariant javascriptResult;
        QEventLoop loop;
        view->page()->runJavaScript(script, [&](const QVariant &result) {
            javascriptResult = result;
            loop.quit();
        });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        return javascriptResult;
    };
    QVERIFY(runJavascript(QStringLiteral("Boolean(document.getElementById('route'))")).toBool());
    QVERIFY(runJavascript(QStringLiteral("Boolean(document.getElementById('zoomControls'))")).toBool());
    QVERIFY(runJavascript(QStringLiteral(
        "document.getElementById('scaleControl').compareDocumentPosition("
        "document.getElementById('zoomControls')) & Node.DOCUMENT_POSITION_FOLLOWING")).toBool());
    const QString mapText = runJavascript(QStringLiteral("document.body.innerText")).toString();
    QVERIFY(mapText.contains(QStringLiteral("路线详情")));
    QVERIFY(mapText.contains(QStringLiteral("出行方式：驾车")));
    QVERIFY(mapText.contains(QStringLiteral("路线距离：15.2 公里")));
    QVERIFY(mapText.contains(QStringLiteral("起点测试地址")));
    QVERIFY(mapText.contains(QStringLiteral("终点测试地址")));
    QVERIFY(mapText.contains(QStringLiteral("OpenStreetMap contributors")));
    QVERIFY(!mapText.contains(QStringLiteral("Log In")));
    QVERIFY(!mapText.contains(QStringLiteral("Directions")));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("100%"));
    const double initialCenter = runJavascript(
        QStringLiteral("window.navigationMapState.centerX")).toDouble();
    QCOMPARE(runJavascript(QStringLiteral("getComputedStyle(document.getElementById('start')).backgroundColor"))
                 .toString(), QStringLiteral("rgb(35, 119, 70)"));
    runJavascript(QStringLiteral("document.getElementById('zoomIn').click()"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("125%"));
    runJavascript(QStringLiteral(
        "document.getElementById('map').dispatchEvent(new WheelEvent('wheel',"
        "{deltaY:-100,clientX:300,clientY:200,cancelable:true}))"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("150%"));
    QVERIFY(!runJavascript(QStringLiteral("document.getElementById('scaleText').textContent"))
                 .toString().isEmpty());
    const double centerBeforeDrag = runJavascript(
        QStringLiteral("window.navigationMapState.centerX")).toDouble();
    runJavascript(QStringLiteral(
        "const m=document.getElementById('map');"
        "m.dispatchEvent(new PointerEvent('pointerdown',{button:0,pointerId:1,clientX:300,clientY:200}));"
        "m.dispatchEvent(new PointerEvent('pointermove',{button:0,pointerId:1,clientX:360,clientY:200}));"
        "m.dispatchEvent(new PointerEvent('pointerup',{button:0,pointerId:1,clientX:360,clientY:200}))"));
    const double centerAfterDrag = runJavascript(
        QStringLiteral("window.navigationMapState.centerX")).toDouble();
    QVERIFY(!qFuzzyCompare(centerBeforeDrag, centerAfterDrag));
    runJavascript(QStringLiteral("setZoom(2000,300,200)"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("2000%"));
    runJavascript(QStringLiteral("document.getElementById('zoomIn').click();"
                                 "document.getElementById('zoomIn').click()"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("8000%"));
    QCOMPARE(runJavascript(QStringLiteral(
        "setZoom(10000000,300,200);render();document.getElementById('scaleText').textContent"))
                 .toString(), QStringLiteral("5 米"));
    const QString maximumPercent = runJavascript(
        QStringLiteral("document.getElementById('zoomPercent').textContent")).toString();
    QVERIFY(runJavascript(QStringLiteral("document.getElementById('zoomIn').disabled")).toBool());
    runJavascript(QStringLiteral(
        "document.getElementById('zoomIn').click();"
        "document.getElementById('map').dispatchEvent(new WheelEvent('wheel',"
        "{deltaY:-100,clientX:300,clientY:200,cancelable:true}))"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), maximumPercent);
    runJavascript(QStringLiteral("document.getElementById('resetZoom').click()"));
    QCOMPARE(runJavascript(QStringLiteral("document.getElementById('zoomPercent').textContent"))
                 .toString(), QStringLiteral("100%"));
    QVERIFY(qFuzzyCompare(runJavascript(QStringLiteral("window.navigationMapState.centerX"))
                              .toDouble(), initialCenter));
    const QString screenshot = qEnvironmentVariable("EV_MAP_TEST_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QTest::qWait(100);
        QVERIFY(dialog->grab().save(screenshot));
    }
    mode->setCurrentIndex(1);
    QTRY_COMPARE(routeRequestCount, 2);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 10000);
    QCOMPARE(lastCosting, QStringLiteral("pedestrian"));
    mode->setCurrentIndex(2);
    QTRY_COMPARE(routeRequestCount, 3);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 10000);
    QCOMPARE(lastCosting, QStringLiteral("bicycle"));
    fail = true;
    const int requestCountBeforeFailure = routeRequestCount;
    mode->setCurrentIndex(0);
    QTRY_VERIFY(routeRequestCount > requestCountBeforeFailure);
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("失败")), 10000);
    QVERIFY(view->isHidden());
    fail = false;
    const int requestCountBeforeRecovery = routeRequestCount;
    mode->setCurrentIndex(1);
    QTRY_VERIFY(routeRequestCount > requestCountBeforeRecovery);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 10000);
    QVERIFY(!view->isHidden());
    dialog->close();
    QTRY_VERIFY(dialog.isNull());

    const int requestCountBeforeLongWalk = routeRequestCount;
    QPointer<NavigationMapDialog> longWalk = new NavigationMapDialog(
        116.3913, 39.9057, 117.2, 39.13, QStringLiteral("北京市"),
        QStringLiteral("天津市测试站"), QStringLiteral("天津市"), 1, &dialogParent);
    auto *longWalkView = longWalk->findChild<QWebEngineView *>(QStringLiteral("embeddedMapView"));
    QVERIFY(longWalkView);
    longWalk->show();
    QTRY_COMPARE_WITH_TIMEOUT(routeRequestCount, requestCountBeforeLongWalk + 2, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!longWalkView->isHidden(), 10000);
    QCOMPARE(lastCosting, QStringLiteral("pedestrian"));
    longWalk->close();
    QTRY_VERIFY(longWalk.isNull());

    const int requestCountBeforeLongRide = routeRequestCount;
    QPointer<NavigationMapDialog> longRide = new NavigationMapDialog(
        116.3913, 39.9057, 117.2, 39.13, QStringLiteral("北京市"),
        QStringLiteral("天津市测试站"), QStringLiteral("天津市"), 2, &dialogParent);
    auto *longRideView = longRide->findChild<QWebEngineView *>(QStringLiteral("embeddedMapView"));
    QVERIFY(longRideView);
    longRide->show();
    QTRY_COMPARE_WITH_TIMEOUT(routeRequestCount, requestCountBeforeLongRide + 2, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!longRideView->isHidden(), 10000);
    QCOMPARE(lastCosting, QStringLiteral("bicycle"));
    longRide->close();
    QTRY_VERIFY(longRide.isNull());
    qunsetenv("EV_NAVIGATION_ROUTE_ENDPOINT");
    qunsetenv("EV_NAVIGATION_TILE_TEMPLATE");
}

void OrderUiTests::realBeijingRouteService()
{
    if (!qEnvironmentVariableIsSet("EV_MAP_REAL_ROUTE_TEST")) {
        QSKIP("Set EV_MAP_REAL_ROUTE_TEST=1 to run the external route integration check");
    }
    qunsetenv("EV_NAVIGATION_ROUTE_ENDPOINT");
    qunsetenv("EV_NAVIGATION_TILE_TEMPLATE");
    QPointer<NavigationMapDialog> dialog = new NavigationMapDialog(
        116.391297, 39.905714, 116.292405, 39.958072,
        QStringLiteral("西长安街"), QStringLiteral("北京大学"),
        QStringLiteral("北京市海淀区颐和园路5号"), 0);
    auto *status = dialog->findChild<QLabel *>(QStringLiteral("mapLoadStatus"));
    auto *view = dialog->findChild<QWebEngineView *>(QStringLiteral("embeddedMapView"));
    auto *mode = dialog->findChild<QComboBox *>(QStringLiteral("navigationMode"));
    QVERIFY(status && view && mode);
    dialog->show();
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 30000);
    mode->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 30000);
    QVERIFY2(status->isHidden(), qPrintable(status->text()));
    mode->setCurrentIndex(2);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isHidden(), 30000);
    QVERIFY2(status->isHidden(), qPrintable(status->text()));
    QTest::qWait(2500);
    const QString screenshot = qEnvironmentVariable("EV_MAP_REAL_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QVERIFY(dialog->grab().save(screenshot));
    }
    dialog->close();
    QTRY_VERIFY(dialog.isNull());
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    OrderUiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "tst_order_ui.moc"
