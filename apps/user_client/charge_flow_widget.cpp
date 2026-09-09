#include "charge_flow_widget.h"

#include "user_api_client.h"
#include "client_ui/apple_widgets.h"
#include "user_session_state.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString pileTypeText(const QString &type)
{
    return type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充");
}

QString yuanText(qint64 cent)
{
    return QStringLiteral("¥%1").arg(cent / 100.0, 0, 'f', 2);
}

QString durationText(qint64 seconds)
{
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QLabel *makeTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("uiClass", "pageTitle");
    return label;
}

} // namespace

ChargeFlowWidget::ChargeFlowWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 8, 0, 0);
    root->setSpacing(20);
    auto *steps = new QHBoxLayout;
    steps->setSpacing(4);
    const QStringList captions {QStringLiteral("01  确认设备"), QStringLiteral("02  预约就绪"),
        QStringLiteral("03  正在充电"), QStringLiteral("04  订单结算")};
    for (const QString &caption : captions) {
        auto *label = new QLabel(caption, this);
        label->setProperty("uiClass", "flowStep");
        stageLabels_.append(label);
        steps->addWidget(label, 1);
    }
    root->addLayout(steps);
    pages_ = new QStackedWidget(this);
    root->addWidget(pages_);

    // ── Idle / checking page (UML-025 entry) ────────────────────────────
    idlePage_ = new QWidget(pages_);
    auto *idleLayout = new QVBoxLayout(idlePage_);
    idleLayout->addWidget(makeTitle(QStringLiteral("充电"), idlePage_));
    idleLabel_ = new QLabel(idlePage_);
    idleLabel_->setProperty("uiClass", "muted");
    idleLabel_->setWordWrap(true);
    idleLayout->addWidget(idleLabel_);
    idleLayout->addStretch();
    pages_->addWidget(idlePage_);

    // ── Pile confirmation page (UML-026) ────────────────────────────────
    selectPage_ = new QWidget(pages_);
    auto *selectLayout = new QVBoxLayout(selectPage_);
    selectLayout->addWidget(makeTitle(QStringLiteral("确认充电桩"), selectPage_));
    selectLayout->setSpacing(18);
    auto *selectionCard = new QFrame(selectPage_);
    selectionCard->setProperty("uiClass", "card");
    auto *cardLayout = new QVBoxLayout(selectionCard);
    cardLayout->setContentsMargins(26, 26, 26, 24);
    cardLayout->setSpacing(22);
    auto *summary = new QHBoxLayout;
    summary->setSpacing(28);
    auto *glyph = new QLabel(selectionCard);
    glyph->setPixmap(ev::appSymbolIcon(ev::AppSymbol::Bolt).pixmap(64, 64));
    glyph->setFixedSize(110, 116);
    glyph->setProperty("uiClass", "chargeGlyph");
    glyph->setAlignment(Qt::AlignCenter);
    summary->addWidget(glyph);
    auto *information = new QVBoxLayout;
    information->setSpacing(14);
    auto *caption = new QLabel(QStringLiteral("已选择的充电设备"), selectionCard);
    caption->setProperty("uiClass", "eyebrow");
    information->addWidget(caption);
    selectInfo_ = new QLabel(selectionCard);
    selectInfo_->setProperty("uiClass", "chargeInfo");
    selectInfo_->setWordWrap(true);
    information->addWidget(selectInfo_);
    selectPrice_ = new QLabel(selectionCard);
    selectPrice_->setProperty("uiClass", "chargePrice");
    information->addWidget(selectPrice_);
    auto *instruction = new QLabel(QStringLiteral("核对站点与设备，预约成功后可开始充电。"), selectionCard);
    instruction->setProperty("uiClass", "muted");
    instruction->setWordWrap(true);
    information->addWidget(instruction);
    summary->addLayout(information, 1);
    cardLayout->addLayout(summary);
    auto *selectButtons = new QHBoxLayout;
    reselectButton_ = new QPushButton(QStringLiteral("返回重新选桩"), selectPage_);
    reselectButton_->setProperty("uiClass", "text");
    selectButtons->addWidget(reselectButton_);
    selectButtons->addStretch();
    reserveButton_ = new QPushButton(QStringLiteral("预约充电"), selectPage_);
    reserveButton_->setProperty("uiClass", "primary");
    selectButtons->addWidget(reserveButton_);
    cardLayout->addLayout(selectButtons);
    selectLayout->addWidget(selectionCard);
    selectLayout->addStretch();
    pages_->addWidget(selectPage_);

    // ── Progress page: reserved and charging share one page ─────────────
    progressPage_ = new QWidget(pages_);
    auto *progressLayout = new QVBoxLayout(progressPage_);
    progressTitle_ = new QLabel(progressPage_);
    progressTitle_->setProperty("uiClass", "pageTitle");
    progressLayout->addWidget(progressTitle_);
    progressInfo_ = new QLabel(progressPage_);
    progressInfo_->setWordWrap(true);
    progressLayout->addWidget(progressInfo_);
    progressPrice_ = new QLabel(progressPage_);
    progressPrice_->setProperty("uiClass", "muted");
    progressLayout->addWidget(progressPrice_);
    hintLabel_ = new QLabel(
        QStringLiteral("充电桩已为您保留，请点击“开始充电”。"), progressPage_);
    hintLabel_->setProperty("uiClass", "muted");
    hintLabel_->setWordWrap(true);
    progressLayout->addWidget(hintLabel_);

    progressBar_ = new QProgressBar(progressPage_);
    progressBar_->setRange(0, 100);
    progressLayout->addWidget(progressBar_);

    metricsCard_ = new QFrame(progressPage_);
    metricsCard_->setProperty("uiClass", "card");
    auto *metricsLayout = new QHBoxLayout(metricsCard_);
    metricsLayout->setContentsMargins(16, 12, 16, 12);
    auto addMetric = [this, metricsLayout](const QString &caption, QLabel **valueLabel) {
        auto *column = new QVBoxLayout;
        auto *captionLabel = new QLabel(caption, metricsCard_);
        captionLabel->setProperty("uiClass", "formLabel");
        *valueLabel = new QLabel(QStringLiteral("--"), metricsCard_);
        (*valueLabel)->setProperty("uiClass", "statValue");
        column->addWidget(captionLabel);
        column->addWidget(*valueLabel);
        metricsLayout->addLayout(column, 1);
    };
    addMetric(QStringLiteral("已充电量（度）"), &kwhLabel_);
    addMetric(QStringLiteral("当前费用（元）"), &feeLabel_);
    addMetric(QStringLiteral("已充时长"), &elapsedLabel_);
    progressLayout->addWidget(metricsCard_);

    auto *progressButtons = new QHBoxLayout;
    cancelButton_ = new QPushButton(QStringLiteral("取消预约"), progressPage_);
    cancelButton_->setProperty("uiClass", "danger");
    progressButtons->addWidget(cancelButton_);
    progressButtons->addStretch();
    startButton_ = new QPushButton(QStringLiteral("开始充电"), progressPage_);
    startButton_->setProperty("uiClass", "primary");
    progressButtons->addWidget(startButton_);
    endButton_ = new QPushButton(QStringLiteral("结束充电"), progressPage_);
    endButton_->setProperty("uiClass", "primary");
    progressButtons->addWidget(endButton_);
    progressLayout->addLayout(progressButtons);
    progressLayout->addStretch();
    pages_->addWidget(progressPage_);

    // ── Settlement result page (UML-031, balance sufficient) ────────────
    settlePage_ = new QWidget(pages_);
    auto *settleLayout = new QVBoxLayout(settlePage_);
    settleTitle_ = new QLabel(settlePage_);
    settleTitle_->setProperty("uiClass", "pageTitle");
    settleLayout->addWidget(settleTitle_);
    settleDetail_ = new QLabel(settlePage_);
    settleDetail_->setProperty("uiClass", "stationMeta");
    settleDetail_->setWordWrap(true);
    settleLayout->addWidget(settleDetail_);
    settleLayout->addStretch();
    homeButton_ = new QPushButton(QStringLiteral("返回首页"), settlePage_);
    homeButton_->setProperty("uiClass", "primary");
    settleLayout->addWidget(homeButton_, 0, Qt::AlignRight);
    pages_->addWidget(settlePage_);

    // ── Pending-settlement page (UML-031, insufficient balance) ─────────
    pendingPage_ = new QWidget(pages_);
    auto *pendingLayout = new QVBoxLayout(pendingPage_);
    pendingTitle_ = new QLabel(pendingPage_);
    pendingTitle_->setProperty("uiClass", "pageTitle");
    pendingLayout->addWidget(pendingTitle_);
    pendingDetail_ = new QLabel(pendingPage_);
    pendingDetail_->setProperty("uiClass", "errorText");
    pendingDetail_->setWordWrap(true);
    pendingLayout->addWidget(pendingDetail_);
    pendingLayout->addStretch();
    auto *pendingButtons = new QHBoxLayout;
    retryButton_ = new QPushButton(QStringLiteral("重新结算"), pendingPage_);
    retryButton_->setProperty("uiClass", "secondary");
    pendingButtons->addWidget(retryButton_);
    rechargeButton_ = new QPushButton(QStringLiteral("去充值"), pendingPage_);
    rechargeButton_->setProperty("uiClass", "primary");
    pendingButtons->addWidget(rechargeButton_);
    pendingLayout->addLayout(pendingButtons);
    pages_->addWidget(pendingPage_);

    elapsedTimer_ = new QTimer(this);
    elapsedTimer_->setInterval(1000);
    connect(elapsedTimer_, &QTimer::timeout, this, &ChargeFlowWidget::updateElapsedLabel);

    connect(api_, &ev::UserApiClient::chargeUpdateReceived,
            this, &ChargeFlowWidget::onChargeUpdate);
    connect(reserveButton_, &QPushButton::clicked, this, &ChargeFlowWidget::reservePile);
    connect(reselectButton_, &QPushButton::clicked, this, [this] {
        reset();
        emit pileSelectionRequested();
    });
    connect(startButton_, &QPushButton::clicked, this, &ChargeFlowWidget::startCharging);
    connect(cancelButton_, &QPushButton::clicked, this, &ChargeFlowWidget::cancelReservation);
    connect(endButton_, &QPushButton::clicked, this, &ChargeFlowWidget::finishCharging);
    connect(homeButton_, &QPushButton::clicked, this, [this] {
        reset();
        emit homeRequested();
    });
    connect(rechargeButton_, &QPushButton::clicked, this, [this] {
        emit rechargeRequested();
    });
    connect(retryButton_, &QPushButton::clicked, this, &ChargeFlowWidget::retrySettlement);

    reset();
}

void ChargeFlowWidget::enterWithPile(qint64 pileId)
{
    if (stage_ != Stage::Idle) {
        QMessageBox::information(this, QStringLiteral("充电"),
            QStringLiteral("您有正在进行的充电流程，请先处理当前订单。"));
        return;
    }
    runPendingCheck(pileId);
}

void ChargeFlowWidget::enterFromHome()
{
    if (stage_ == Stage::Idle) {
        runPendingCheck(0);
    }
}

void ChargeFlowWidget::reset()
{
    setStage(Stage::Idle);
    orderId_ = 0;
    pileId_ = 0;
    startTimeText_.clear();
    stationName_.clear();
    pileNumber_.clear();
    pileType_.clear();
    powerKw_ = 0.0;
    pricePerKwh_ = 0.0;
    membershipLevel_ = QStringLiteral("NORMAL");
    discountBps_ = 10000;
    idleLabel_->setText(QStringLiteral("进入充电流程后将自动检查未完成订单。"));
    pages_->setCurrentWidget(idlePage_);
}

void ChargeFlowWidget::setStage(Stage stage)
{
    stage_ = stage;
    const int currentStep = stage == Stage::Reserved ? 1 : stage == Stage::Charging ? 2
        : (stage == Stage::Settled || stage == Stage::PendingSettlement) ? 3 : 0;
    for (int i = 0; i < stageLabels_.size(); ++i) {
        stageLabels_[i]->setProperty("active", i == currentStep);
        stageLabels_[i]->style()->unpolish(stageLabels_[i]);
        stageLabels_[i]->style()->polish(stageLabels_[i]);
    }
    if (stage_ != Stage::Charging) {
        elapsedTimer_->stop();
    }
}

void ChargeFlowWidget::runPendingCheck(qint64 pileId)
{
    setStage(Stage::Checking);
    idleLabel_->setText(QStringLiteral("正在检查未完成订单…"));
    pages_->setCurrentWidget(idlePage_);
    api_->checkPendingCharge(this,
        [this, pileId](bool success, const QJsonObject &result, const QString &message) {
            if (stage_ != Stage::Checking) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("检查失败"), message);
                reset();
                return;
            }
            if (result.value(QStringLiteral("has_pending")).toBool()) {
                applyPendingOrder(result.value(QStringLiteral("order")).toObject());
                return;
            }
            if (pileId > 0) {
                verifyPile(pileId);
                return;
            }
            QMessageBox::information(this, QStringLiteral("充电"),
                QStringLiteral("当前没有未完成的充电订单。请先在“找桩”页选择充电桩。"));
            reset();
            emit pileSelectionRequested();
        });
}

void ChargeFlowWidget::verifyPile(qint64 pileId)
{
    idleLabel_->setText(QStringLiteral("正在校验充电桩状态…"));
    api_->checkPile(pileId, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            if (stage_ != Stage::Checking) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("校验失败"), message);
                reset();
                return;
            }
            if (!result.value(QStringLiteral("available")).toBool()) {
                const QString reason = result.value(QStringLiteral("reason")).toString();
                QMessageBox::warning(this, QStringLiteral("无法预约"),
                    reason.isEmpty()
                        ? QStringLiteral("该充电桩不可用，请选择其他桩")
                        : reason);
                reset();
                emit pileSelectionRequested();
                return;
            }
            const QJsonObject pile = result.value(QStringLiteral("pile")).toObject();
            pileId_ = pile.value(QStringLiteral("pile_id")).toInteger();
            pileNumber_ = pile.value(QStringLiteral("pile_number")).toString();
            pileType_ = pile.value(QStringLiteral("pile_type")).toString();
            powerKw_ = pile.value(QStringLiteral("power_kw")).toDouble();
            stationName_ = result.value(QStringLiteral("station_name")).toString();
            pricePerKwh_ = result.value(QStringLiteral("price_per_kwh")).toDouble();
            renderPileInfo(selectInfo_, selectPrice_);
            setStage(Stage::SelectPile);
            pages_->setCurrentWidget(selectPage_);
        });
}

void ChargeFlowWidget::renderPileInfo(QLabel *infoLabel, QLabel *priceLabel) const
{
    infoLabel->setText(QStringLiteral("%1 · %2（%3，%4 kW）")
                           .arg(stationName_, pileNumber_, pileTypeText(pileType_))
                           .arg(powerKw_, 0, 'f', 1));
    QString text = QStringLiteral("充电原价 ¥%1 / 度").arg(pricePerKwh_, 0, 'f', 2);
    if (orderId_ > 0) {
        text += discountBps_ < 10000
            ? QStringLiteral(" · 本单 %1 %2折（预约时锁定）").arg(membershipLevel_).arg(discountBps_ / 1000.0, 0, 'g', 4)
            : QStringLiteral(" · 本单无会员优惠");
    } else {
        text += QStringLiteral(" · 会员优惠将在预约时确定");
    }
    priceLabel->setText(text);
}

void ChargeFlowWidget::reservePile()
{
    reserveButton_->setEnabled(false);
    api_->reserveCharge(pileId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            reserveButton_->setEnabled(true);
            if (stage_ != Stage::SelectPile) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("预约失败"), message);
                reset();
                return;
            }
            orderId_ = result.value(QStringLiteral("order_id")).toInteger();
            membershipLevel_ = result.value("membership_level").toString("NORMAL");
            discountBps_ = result.value("discount_bps").toInt(10000);
            QMessageBox::information(this, QStringLiteral("预约成功"),
                QStringLiteral("充电桩已为您保留，请点击“开始充电”。"));
            showReservedPage();
        });
}

void ChargeFlowWidget::startCharging()
{
    startButton_->setEnabled(false);
    api_->startCharge(orderId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            startButton_->setEnabled(true);
            if (stage_ != Stage::Reserved) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("开始失败"), message);
                return;
            }
            startTimeText_ = result.value(QStringLiteral("start_time")).toString();
            powerKw_ = result.value(QStringLiteral("power_kw")).toDouble(powerKw_);
            pricePerKwh_ = result.value(QStringLiteral("price_per_kwh")).toDouble(pricePerKwh_);
            membershipLevel_ = result.value("membership_level").toString(membershipLevel_);
            discountBps_ = result.value("discount_bps").toInt(discountBps_);
            showChargingPage();
        });
}

void ChargeFlowWidget::finishCharging()
{
    const auto answer = QMessageBox::question(this, QStringLiteral("结束充电"),
        QStringLiteral("确定结束充电并结算吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    endButton_->setEnabled(false);
    api_->endCharge(orderId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            endButton_->setEnabled(true);
            if (stage_ != Stage::Charging) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("结算失败"), message);
                return;
            }
            if (result.value(QStringLiteral("settled")).toBool()) {
                showSettledPage(result);
            } else {
                showPendingPage(result);
            }
        });
}

void ChargeFlowWidget::retrySettlement()
{
    retryButton_->setEnabled(false);
    api_->endCharge(orderId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            retryButton_->setEnabled(true);
            if (stage_ != Stage::PendingSettlement) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("结算失败"), message);
                return;
            }
            if (result.value(QStringLiteral("settled")).toBool()) {
                showSettledPage(result);
            } else {
                showPendingPage(result);
            }
        });
}

void ChargeFlowWidget::cancelReservation()
{
    const auto answer = QMessageBox::question(this, QStringLiteral("取消预约"),
        QStringLiteral("确定取消预约吗？充电桩将释放给其他用户。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    cancelButton_->setEnabled(false);
    api_->cancelCharge(orderId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            cancelButton_->setEnabled(true);
            if (stage_ != Stage::Reserved) {
                return;
            }
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("取消失败"), message);
                return;
            }
            QMessageBox::information(this, QStringLiteral("已取消"),
                                     QStringLiteral("预约已取消。"));
            reset();
            emit homeRequested();
        });
}

void ChargeFlowWidget::applyPendingOrder(const QJsonObject &order)
{
    orderId_ = order.value(QStringLiteral("order_id")).toInteger();
    stationName_ = order.value(QStringLiteral("station_name")).toString();
    pileNumber_ = order.value(QStringLiteral("pile_number")).toString();
    pileType_ = order.value(QStringLiteral("pile_type")).toString();
    powerKw_ = order.value(QStringLiteral("power_kw")).toDouble();
    pricePerKwh_ = order.value(QStringLiteral("price_per_kwh")).toDouble();
    membershipLevel_ = order.value("membership_level").toString("NORMAL");
    discountBps_ = order.value("discount_bps").toInt(10000);
    const QString status = order.value(QStringLiteral("status")).toString();

    QMessageBox::information(this, QStringLiteral("未完成订单"),
        QStringLiteral("检测到未完成的充电订单，已为您恢复进度。"));

    if (status == QStringLiteral("reserved")) {
        showReservedPage();
        return;
    }
    if (status == QStringLiteral("charging")) {
        startTimeText_ = order.value(QStringLiteral("start_time")).toString();
        showChargingPage();
        const QJsonObject live = order.value(QStringLiteral("live")).toObject();
        if (!live.isEmpty()) {
            kwhLabel_->setText(QStringLiteral("%1").arg(
                live.value(QStringLiteral("charge_amount_kwh")).toDouble(), 0, 'f', 2));
            feeLabel_->setText(QStringLiteral("%1").arg(
                live.value(QStringLiteral("current_fee_cent")).toInteger() / 100.0,
                0, 'f', 2));
            progressBar_->setValue(qBound(0, qRound(
                live.value(QStringLiteral("progress")).toDouble()), 100));
        }
        return;
    }

    // pending_settlement: restore stored values, balance from local session.
    const qint64 feeCent = order.value(QStringLiteral("total_fee_cent")).toInteger();
    const qint64 balanceCent = UserSessionState::instance().balanceCent();
    showPendingPage(QJsonObject {
        {QStringLiteral("total_kwh"),
         order.value(QStringLiteral("charge_amount_kwh")).toDouble()},
        {QStringLiteral("total_fee_cent"), feeCent},
        {QStringLiteral("gross_fee_cent"), order.value("gross_fee_cent").toInteger(feeCent)},
        {QStringLiteral("balance_cent"), balanceCent},
        {QStringLiteral("shortfall_cent"), qMax<qint64>(0, feeCent - balanceCent)}
    });
}

void ChargeFlowWidget::onChargeUpdate(const QJsonObject &update)
{
    if (stage_ != Stage::Charging
        || update.value(QStringLiteral("order_id")).toInteger() != orderId_) {
        return;
    }
    kwhLabel_->setText(QStringLiteral("%1").arg(
        update.value(QStringLiteral("charge_amount_kwh")).toDouble(), 0, 'f', 2));
    feeLabel_->setText(QStringLiteral("%1").arg(
        update.value(QStringLiteral("current_fee_cent")).toInteger() / 100.0, 0, 'f', 2));
    progressBar_->setValue(qBound(0, qRound(
        update.value(QStringLiteral("progress")).toDouble()), 100));
}

void ChargeFlowWidget::showReservedPage()
{
    setStage(Stage::Reserved);
    progressTitle_->setText(QStringLiteral("预约成功"));
    renderPileInfo(progressInfo_, progressPrice_);
    hintLabel_->setVisible(true);
    progressBar_->setVisible(false);
    metricsCard_->setVisible(false);
    startButton_->setVisible(true);
    cancelButton_->setVisible(true);
    endButton_->setVisible(false);
    pages_->setCurrentWidget(progressPage_);
}

void ChargeFlowWidget::showChargingPage()
{
    setStage(Stage::Charging);
    progressTitle_->setText(QStringLiteral("正在充电"));
    renderPileInfo(progressInfo_, progressPrice_);
    hintLabel_->setVisible(false);
    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    metricsCard_->setVisible(true);
    kwhLabel_->setText(QStringLiteral("0.00"));
    feeLabel_->setText(QStringLiteral("0.00"));
    elapsedLabel_->setText(QStringLiteral("00:00:00"));
    startButton_->setVisible(false);
    cancelButton_->setVisible(false);
    endButton_->setVisible(true);
    pages_->setCurrentWidget(progressPage_);
    elapsedTimer_->start();
    updateElapsedLabel();
}

void ChargeFlowWidget::showSettledPage(const QJsonObject &result)
{
    setStage(Stage::Settled);
    const qint64 balanceCent = result.value(QStringLiteral("balance_cent")).toInteger();
    UserSessionState::instance().setBalanceCent(balanceCent);
    settleTitle_->setText(QStringLiteral("充电完成"));
    const auto net = result.value("total_fee_cent").toInteger();
    const auto gross = result.value("gross_fee_cent").toInteger(net);
    settleDetail_->setText(QStringLiteral("充电量 %1 度 · 原价 %2 · 优惠 %3\n实付 %4 · 当前余额 %5")
        .arg(result.value(QStringLiteral("total_kwh")).toDouble(), 0, 'f', 2)
        .arg(yuanText(gross), yuanText(gross - net), yuanText(net), yuanText(balanceCent)));
    pages_->setCurrentWidget(settlePage_);
}

void ChargeFlowWidget::showPendingPage(const QJsonObject &result)
{
    setStage(Stage::PendingSettlement);
    const qint64 feeCent = result.value(QStringLiteral("total_fee_cent")).toInteger();
    const qint64 balanceCent = result.value(QStringLiteral("balance_cent")).toInteger();
    const qint64 shortfallCent = result.value(QStringLiteral("shortfall_cent")).toInteger();
    pendingTitle_->setText(QStringLiteral("余额不足，订单待结算"));
    pendingDetail_->setText(QStringLiteral(
        "充电已完成，但余额不足以支付本次费用。\n"
        "充电量 %1 度 · 应付 %2 · 当前余额 %3 · 差额 %4\n"
        "原价 %5 · 优惠 %6（预约时锁定）\n"
        "请先充值，然后返回本页点击“重新结算”；结算前充电桩保持占用。")
        .arg(result.value(QStringLiteral("total_kwh")).toDouble(), 0, 'f', 2)
        .arg(yuanText(feeCent), yuanText(balanceCent), yuanText(shortfallCent),
             yuanText(result.value("gross_fee_cent").toInteger(feeCent)),
             yuanText(result.value("gross_fee_cent").toInteger(feeCent) - feeCent)));
    pages_->setCurrentWidget(pendingPage_);
}

void ChargeFlowWidget::updateElapsedLabel()
{
    const QDateTime start = QDateTime::fromString(
        startTimeText_, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    if (!start.isValid()) {
        return;
    }
    const qint64 seconds = qMax<qint64>(0, start.secsTo(QDateTime::currentDateTime()));
    elapsedLabel_->setText(durationText(seconds));
}
