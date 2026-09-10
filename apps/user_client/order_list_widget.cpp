#include "order_list_widget.h"

#include "user_api_client.h"
#include "user_session_state.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QLabel *plainLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

QString displayTime(const QJsonValue &value)
{
    return value.toString().isEmpty() ? QStringLiteral("—") : value.toString();
}

} // namespace

OrderListWidget::OrderListWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 8, 0, 0);
    auto *header = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("< 返回首页"), this);
    back->setProperty("uiClass", "text");
    header->addWidget(back);
    auto *title = plainLabel(QStringLiteral("我的订单"), this);
    title->setProperty("uiClass", "pageTitle");
    header->addWidget(title, 1);
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setObjectName(QStringLiteral("refreshOrdersButton"));
    refreshButton_->setProperty("uiClass", "primary");
    header->addWidget(refreshButton_);
    root->addLayout(header);
    statusLabel_ = plainLabel(QStringLiteral("进入页面后查询订单"), this);
    statusLabel_->setObjectName(QStringLiteral("ordersStatus"));
    root->addWidget(statusLabel_);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *cards = new QWidget(scroll);
    cardsLayout_ = new QVBoxLayout(cards);
    cardsLayout_->setContentsMargins(0, 4, 8, 4);
    cardsLayout_->setSpacing(12);
    cardsLayout_->addStretch();
    scroll->setWidget(cards);
    root->addWidget(scroll, 1);
    connect(back, &QPushButton::clicked, this, &OrderListWidget::backRequested);
    connect(refreshButton_, &QPushButton::clicked, this, &OrderListWidget::refresh);
    connect(api_, &ev::UserApiClient::sessionExpired, this, &OrderListWidget::reset);
}

void OrderListWidget::clearCards()
{
    while (cardsLayout_->count() > 1) {
        auto *item = cardsLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
}

void OrderListWidget::reset()
{
    ++requestGeneration_;
    clearCards();
    refreshButton_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("进入页面后查询订单"));
}

void OrderListWidget::refresh()
{
    const auto generation = ++requestGeneration_;
    const QString sessionId = UserSessionState::instance().sessionId();
    clearCards();
    refreshButton_->setEnabled(false);
    statusLabel_->setText(QStringLiteral("正在查询订单…"));
    api_->queryOrders(this, [this, generation, sessionId](
        bool success, const QJsonObject &result, const QString &message) {
        if (generation != requestGeneration_
            || sessionId != UserSessionState::instance().sessionId()) {
            return;
        }
        refreshButton_->setEnabled(true);
        if (!success || !result.value(QStringLiteral("orders")).isArray()) {
            statusLabel_->setText(message.isEmpty()
                ? QStringLiteral("订单查询失败，请点击刷新重试")
                : QStringLiteral("%1；请点击刷新重试").arg(message));
            return;
        }
        renderOrders(result.value(QStringLiteral("orders")).toArray());
    });
}

void OrderListWidget::renderOrders(const QJsonArray &orders)
{
    statusLabel_->setText(orders.isEmpty() ? QStringLiteral("暂无充电订单")
        : QStringLiteral("共 %1 笔订单 · 按预约时间从新到旧排列").arg(orders.size()));
    for (const auto &value : orders) {
        const auto order = value.toObject();
        auto *card = new QFrame(this);
        card->setProperty("uiClass", "card");
        card->setObjectName(QStringLiteral("orderCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 14, 18, 14);
        auto *heading = new QHBoxLayout;
        auto *station = plainLabel(order.value(QStringLiteral("station_name")).toString(), card);
        station->setProperty("uiClass", "cardTitle");
        heading->addWidget(station, 1);
        auto *badge = plainLabel(order.value(QStringLiteral("status_text")).toString(), card);
        badge->setProperty("uiClass", "orderStatus");
        badge->setProperty("orderStatus", order.value(QStringLiteral("status")).toString());
        heading->addWidget(badge);
        layout->addLayout(heading);
        layout->addWidget(plainLabel(QStringLiteral("订单号：%1    充电桩：%2")
            .arg(order.value(QStringLiteral("order_id")).toInteger())
            .arg(order.value(QStringLiteral("pile_number")).toString()), card));
        layout->addWidget(plainLabel(QStringLiteral("充电量：%1度    费用：¥%2    单价：¥%3/度")
            .arg(order.value(QStringLiteral("charge_amount_kwh")).toDouble(), 0, 'f', 2)
            .arg(order.value(QStringLiteral("total_fee")).toDouble(), 0, 'f', 2)
            .arg(order.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2), card));
        layout->addWidget(plainLabel(QStringLiteral("预约时间：%1")
            .arg(displayTime(order.value(QStringLiteral("reserve_time")))), card));
        if (order.value("discount_bps").toInt(10000) < 10000) {
            layout->addWidget(plainLabel(QStringLiteral("本单 %1 %2折 · 原价 ¥%3 · 已优惠 ¥%4（预约时锁定）")
                .arg(order.value("membership_level").toString())
                .arg(order.value("discount_bps").toInt() / 1000.0, 0, 'g', 4)
                .arg(order.value("gross_fee_cent").toInteger() / 100.0, 0, 'f', 2)
                .arg(order.value("discount_fee_cent").toInteger() / 100.0, 0, 'f', 2), card));
        }
        if (order.value(QStringLiteral("status")).toString() == QStringLiteral("pending_settlement")) {
            auto *hint = plainLabel(QStringLiteral("此订单待结算，请前往充电流程完成结算。"), card);
            hint->setProperty("uiClass", "orderSettlementHint");
            layout->addWidget(hint);
        }
        const auto duration = order.value(QStringLiteral("duration_hours"));
        auto *details = plainLabel(QStringLiteral("开始时间：%1\n结束时间：%2\n充电时长：%3")
            .arg(displayTime(order.value(QStringLiteral("start_time"))),
                 displayTime(order.value(QStringLiteral("end_time"))),
                 duration.isDouble() ? QStringLiteral("%1 小时").arg(duration.toDouble(), 0, 'f', 2)
                                     : QStringLiteral("—（尚无完整起止时间）")), card);
        details->setObjectName(QStringLiteral("orderDetails"));
        layout->addWidget(details);
        details->hide();
        auto *toggle = new QPushButton(QStringLiteral("查看详情"), card);
        toggle->setCheckable(true);
        toggle->setProperty("uiClass", "text");
        layout->addWidget(toggle, 0, Qt::AlignLeft);
        connect(toggle, &QPushButton::toggled, details, [details, toggle](bool expanded) {
            details->setVisible(expanded);
            toggle->setText(expanded ? QStringLiteral("收起详情") : QStringLiteral("查看详情"));
        });
        cardsLayout_->insertWidget(cardsLayout_->count() - 1, card);
    }
}
