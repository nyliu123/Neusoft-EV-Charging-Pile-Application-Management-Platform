#include "charge_flow_widget.h"

#include "user_api_client.h"
#include "user_session_state.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
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
    root->setContentsMargins(24, 20, 24, 20);
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
    selectInfo_ = new QLabel(selectPage_);
    selectInfo_->setWordWrap(true);
    selectLayout->addWidget(selectInfo_);
    selectPrice_ = new QLabel(selectPage_);
    selectPrice_->setProperty("uiClass", "muted");
    selectLayout->addWidget(selectPrice_);
    selectLayout->addStretch();
    auto *selectButtons = new QHBoxLayout;
    reselectButton_ = new QPushButton(QStringLiteral("返回重新选桩"), selectPage_);
    reselectButton_->setProperty("uiClass", "text");
    selectButtons->addWidget(reselectButton_);
    selectButtons->addStretch();
    reserveButton_ = new QPushButton(QStringLiteral("预约充电"), selectPage_);
    reserveButton_->setProperty("uiClass", "primary");
    selectButtons->addWidget(reserveButton_);
    selectLayout->addLayout(selectButtons);
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
    idleLabel_->setText(QStringLiteral("进入充电流程后将自动检查未完成订单。"));
    pages_->setCurrentWidget(idlePage_);
}

void ChargeFlowWidget::setStage(Stage stage)
{
    stage_ = stage;
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
    priceLabel->setText(QStringLiteral("充电单价 ¥%1 / 度").arg(pricePerKwh_, 0, 'f', 2));
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
    settleDetail_->setText(QStringLiteral("充电量 %1 度 · 费用 %2 · 当前余额 %3")
        .arg(result.value(QStringLiteral("total_kwh")).toDouble(), 0, 'f', 2)
        .arg(yuanText(result.value(QStringLiteral("total_fee_cent")).toInteger()),
             yuanText(balanceCent)));
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
        "充电量 %1 度 · 费用 %2 · 当前余额 %3 · 差额 %4\n"
        "请先充值，然后返回本页点击“重新结算”；结算前充电桩保持占用。")
        .arg(result.value(QStringLiteral("total_kwh")).toDouble(), 0, 'f', 2)
        .arg(yuanText(feeCent), yuanText(balanceCent), yuanText(shortfallCent)));
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
