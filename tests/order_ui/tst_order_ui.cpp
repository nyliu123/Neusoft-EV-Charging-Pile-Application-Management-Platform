#include "order_list_widget.h"
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
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class OrderUiTests final : public QObject {
    Q_OBJECT
private slots:
    void listDetailsRetryAndStaleResponses();
};

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
    QFile stylesheet(qEnvironmentVariable("EV_ORDER_TEST_STYLESHEET"));
    if (!stylesheet.fileName().isEmpty() && stylesheet.open(QIODevice::ReadOnly)) {
        home.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }
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

QTEST_MAIN(OrderUiTests)
#include "tst_order_ui.moc"
