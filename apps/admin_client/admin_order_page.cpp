#include "admin_order_page.h"

#include "admin_api_client.h"
#include "admin_format.h"
#include "client_ui/animated_combo_box.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ev {

namespace {

constexpr int kColumnOrderId = 0;
constexpr int kColumnUser = 1;
constexpr int kColumnStation = 2;
constexpr int kColumnPile = 3;
constexpr int kColumnStatus = 4;
constexpr int kColumnReserveTime = 5;
constexpr int kColumnChargeTime = 6;
constexpr int kColumnKwh = 7;
constexpr int kColumnPrice = 8;
constexpr int kColumnFee = 9;

enum DatePreset {
    kDateAll = 0,
    kDateToday,
    kDateLast7Days,
    kDateLast30Days,
    kDateCustom
};

} // namespace

AdminOrderPage::AdminOrderPage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // Header.
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    auto *titleLabel = new QLabel(QStringLiteral("订单管理"), this);
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    rootLayout->addLayout(headerLayout);

    // Filter row 1: status + station.
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(10);

    auto *statusLabel = new QLabel(QStringLiteral("订单状态："), this);
    statusLabel->setProperty("uiClass", "formLabel");
    filterLayout->addWidget(statusLabel);
    statusBox_ = new ev::AnimatedComboBox(this);
    statusBox_->setMinimumWidth(130);
    statusBox_->addItem(QStringLiteral("全部状态"), QString());
    statusBox_->addItem(QStringLiteral("已预约"), QStringLiteral("reserved"));
    statusBox_->addItem(QStringLiteral("充电中"), QStringLiteral("charging"));
    statusBox_->addItem(QStringLiteral("待结算"), QStringLiteral("pending_settlement"));
    statusBox_->addItem(QStringLiteral("已完成"), QStringLiteral("settled"));
    statusBox_->addItem(QStringLiteral("已取消"), QStringLiteral("cancelled"));
    filterLayout->addWidget(statusBox_);

    auto *stationLabel = new QLabel(QStringLiteral("充电站："), this);
    stationLabel->setProperty("uiClass", "formLabel");
    filterLayout->addWidget(stationLabel);
    stationBox_ = new ev::AnimatedComboBox(this);
    stationBox_->setMinimumWidth(200);
    stationBox_->addItem(QStringLiteral("全部站点"), 0);
    filterLayout->addWidget(stationBox_);

    filterLayout->addStretch();
    rootLayout->addLayout(filterLayout);

    // Filter row 2: date range + buttons.
    auto *dateLayout = new QHBoxLayout();
    dateLayout->setSpacing(10);

    auto *presetLabel = new QLabel(QStringLiteral("时间范围："), this);
    presetLabel->setProperty("uiClass", "formLabel");
    dateLayout->addWidget(presetLabel);
    datePresetBox_ = new ev::AnimatedComboBox(this);
    datePresetBox_->setMinimumWidth(120);
    datePresetBox_->addItem(QStringLiteral("全部时间"), kDateAll);
    datePresetBox_->addItem(QStringLiteral("今日"), kDateToday);
    datePresetBox_->addItem(QStringLiteral("近7日"), kDateLast7Days);
    datePresetBox_->addItem(QStringLiteral("近30日"), kDateLast30Days);
    datePresetBox_->addItem(QStringLiteral("自定义"), kDateCustom);
    dateLayout->addWidget(datePresetBox_);

    startDateEdit_ = new QDateEdit(QDate::currentDate(), this);
    startDateEdit_->setCalendarPopup(true);
    startDateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    startDateEdit_->setMinimumWidth(180);
    startDateEdit_->setEnabled(false);
    startDateEdit_->setProperty("calendarButtonVisible", false);
    startDateEdit_->calendarWidget()->installEventFilter(this);
    dateLayout->addWidget(startDateEdit_);
    auto *rangeLabel = new QLabel(QStringLiteral("至"), this);
    rangeLabel->setProperty("tone", "muted");
    dateLayout->addWidget(rangeLabel);
    endDateEdit_ = new QDateEdit(QDate::currentDate(), this);
    endDateEdit_->setCalendarPopup(true);
    endDateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    endDateEdit_->setMinimumWidth(180);
    endDateEdit_->setEnabled(false);
    endDateEdit_->setProperty("calendarButtonVisible", false);
    endDateEdit_->calendarWidget()->installEventFilter(this);
    dateLayout->addWidget(endDateEdit_);

    dateLayout->addStretch();

    queryButton_ = new QPushButton(QStringLiteral("查询"), this);
    queryButton_->setProperty("uiClass", "primary");
    dateLayout->addWidget(queryButton_);
    resetButton_ = new QPushButton(QStringLiteral("重置筛选"), this);
    resetButton_->setProperty("uiClass", "secondary");
    dateLayout->addWidget(resetButton_);
    rootLayout->addLayout(dateLayout);

    // Table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(10);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("订单ID"),
        QStringLiteral("用户"),
        QStringLiteral("充电站"),
        QStringLiteral("充电桩"),
        QStringLiteral("状态"),
        QStringLiteral("预约时间"),
        QStringLiteral("充电时间"),
        QStringLiteral("充电量（度）"),
        QStringLiteral("单价"),
        QStringLiteral("总费用")
    });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnOrderId, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnStatus, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnKwh, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPrice, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnFee, QHeaderView::ResizeToContents);
    rootLayout->addWidget(table_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setProperty("tone", "muted");
    rootLayout->addWidget(statusLabel_);

    // Loading overlay — centred in the table area.
    loadingOverlay_ = new QLabel(table_);
    loadingOverlay_->setAlignment(Qt::AlignCenter);
    loadingOverlay_->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(255,255,255,200); color: #409eff;"
        "  font-size: 14px; border-radius: 8px; }"));
    loadingOverlay_->hide();

    applyDatePreset();
    connect(datePresetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyDatePreset(); loadOrders(); });
    connect(queryButton_, &QPushButton::clicked, this, &AdminOrderPage::loadOrders);
    connect(resetButton_, &QPushButton::clicked, this, &AdminOrderPage::resetFilters);
}

void AdminOrderPage::reloadAll()
{
    loadStations();
}

void AdminOrderPage::reload()
{
    loadOrders();
}

void AdminOrderPage::loadStations()
{
    setLoading(true, QStringLiteral("正在加载站点列表..."));
    api_->sendStationOptions(this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            if (!ok) {
                setLoading(false, QString());
                setStatusText(QStringLiteral("站点列表加载失败：%1").arg(message), true);
                loadOrders();
                return;
            }
            const QJsonArray stations = result.value(QStringLiteral("stations")).toArray();
            const QSignalBlocker blocker(stationBox_);
            const int currentId = stationBox_->currentData().toInt();
            stationBox_->clear();
            stationBox_->addItem(QStringLiteral("全部站点"), 0);
            for (const QJsonValue &value : stations) {
                const QJsonObject station = value.toObject();
                stationBox_->addItem(
                    station.value(QStringLiteral("station_name")).toString(),
                    station.value(QStringLiteral("station_id")).toInt());
            }
            const int index = stationBox_->findData(currentId);
            if (index >= 0) {
                stationBox_->setCurrentIndex(index);
            }
            loadOrders();
        });
}

void AdminOrderPage::applyDatePreset()
{
    const int preset = datePresetBox_->currentData().toInt();
    const bool custom = preset == kDateCustom;
    startDateEdit_->setEnabled(custom);
    endDateEdit_->setEnabled(custom);
    setCalendarButtonsVisible(custom);

    const QDate today = QDate::currentDate();
    if (preset != kDateAll && !custom) {
        const QDate start = preset == kDateToday
            ? today
            : today.addDays(preset == kDateLast7Days ? -6 : -29);
        startDateEdit_->setDate(start);
        endDateEdit_->setDate(today);
    }
}

void AdminOrderPage::applyAllDateRange(const QJsonObject &result,
                                       const QJsonArray &orders)
{
    if (datePresetBox_->currentData().toInt() != kDateAll) {
        return;
    }

    QDate firstDate = QDate::fromString(
        result.value(QStringLiteral("first_reserve_date")).toString(), Qt::ISODate);
    QDate lastDate = QDate::fromString(
        result.value(QStringLiteral("last_reserve_date")).toString(), Qt::ISODate);

    // Compatibility with older servers: infer the bounds from an unfiltered
    // order response when the new range fields are absent.
    if (!firstDate.isValid() || !lastDate.isValid()) {
        for (const QJsonValue &value : orders) {
            const QDate reserveDate = QDate::fromString(
                value.toObject().value(QStringLiteral("reserve_time"))
                    .toString().left(10), Qt::ISODate);
            if (!reserveDate.isValid()) {
                continue;
            }
            if (!firstDate.isValid() || reserveDate < firstDate) {
                firstDate = reserveDate;
            }
            if (!lastDate.isValid() || reserveDate > lastDate) {
                lastDate = reserveDate;
            }
        }
    }

    if (firstDate.isValid() && lastDate.isValid()) {
        startDateEdit_->setDate(firstDate);
        endDateEdit_->setDate(lastDate);
    }
}

bool AdminOrderPage::eventFilter(QObject *watched, QEvent *event)
{
    const bool isDateCalendar = watched == startDateEdit_->calendarWidget()
        || watched == endDateEdit_->calendarWidget();
    if (isDateCalendar && event->type() == QEvent::Show) {
        setCalendarButtonsVisible(false);
    } else if (isDateCalendar && event->type() == QEvent::Hide) {
        setCalendarButtonsVisible(
            datePresetBox_->currentData().toInt() == kDateCustom);
    }
    return QWidget::eventFilter(watched, event);
}

void AdminOrderPage::setCalendarButtonsVisible(bool visible)
{
    for (QDateEdit *edit : {startDateEdit_, endDateEdit_}) {
        if (edit->property("calendarButtonVisible").toBool() == visible) {
            continue;
        }
        edit->setProperty("calendarButtonVisible", visible);
        edit->style()->unpolish(edit);
        edit->style()->polish(edit);
        edit->update();
    }
}

void AdminOrderPage::loadOrders()
{
    QJsonObject params;
    const QString status = statusBox_->currentData().toString();
    if (!status.isEmpty()) {
        params.insert(QStringLiteral("status"), status);
    }
    const int stationId = stationBox_->currentData().toInt();
    if (stationId > 0) {
        params.insert(QStringLiteral("station_id"), stationId);
    }
    const int preset = datePresetBox_->currentData().toInt();
    if (preset != kDateAll) {
        params.insert(QStringLiteral("start_date"), startDateEdit_->date().toString(Qt::ISODate));
        params.insert(QStringLiteral("end_date"), endDateEdit_->date().toString(Qt::ISODate));
    }

    setLoading(true, QStringLiteral("正在加载订单列表..."));
    api_->sendQuery(QStringLiteral("order_list"), params, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            setLoading(false, QString());
            if (!ok) {
                table_->setRowCount(0);
                setStatusText(QStringLiteral("订单列表加载失败：%1").arg(message), true);
                return;
            }
            const QJsonArray orders = result.value(QStringLiteral("orders")).toArray();
            applyAllDateRange(result, orders);
            fillTable(orders);
        });
}

void AdminOrderPage::fillTable(const QJsonArray &orders)
{
    table_->setRowCount(0);

    if (orders.isEmpty()) {
        setStatusText(QStringLiteral("没有符合条件的订单记录"), false);
        return;
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(orders.size());

    for (int row = 0; row < orders.size(); ++row) {
        const QJsonObject order = orders[row].toObject();
        const QString status = order.value(QStringLiteral("status")).toString();
        const double kwh = order.value(QStringLiteral("charge_amount_kwh")).toDouble();
        const double fee = order.value(QStringLiteral("total_fee")).toDouble();
        const bool settled = status == QStringLiteral("settled");

        auto *idItem = new QTableWidgetItem(
            QString::number(order.value(QStringLiteral("order_id")).toInt()));
        idItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnOrderId, idItem);

        table_->setItem(row, kColumnUser, new QTableWidgetItem(QStringLiteral("%1（%2）")
            .arg(order.value(QStringLiteral("user_nickname")).toString(),
                 maskPhone(order.value(QStringLiteral("user_phone")).toString()))));

        table_->setItem(row, kColumnStation,
            new QTableWidgetItem(order.value(QStringLiteral("station_name")).toString()));

        table_->setItem(row, kColumnPile, new QTableWidgetItem(QStringLiteral("%1 · %2")
            .arg(order.value(QStringLiteral("pile_number")).toString(),
                 pileTypeText(order.value(QStringLiteral("pile_type")).toString()))));

        auto *statusItem = new QTableWidgetItem(orderStatusText(status));
        statusItem->setForeground(orderStatusColor(status));
        statusItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnStatus, statusItem);

        auto *reserveItem = new QTableWidgetItem(formatDateTimeShort(
            order.value(QStringLiteral("reserve_time")).toString()));
        reserveItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnReserveTime, reserveItem);

        const QString startTime = order.value(QStringLiteral("start_time")).toString();
        const QString endTime = order.value(QStringLiteral("end_time")).toString();
        QString chargeTime = QStringLiteral("—");
        if (!startTime.isEmpty()) {
            chargeTime = endTime.isEmpty()
                ? QStringLiteral("%1 ~ 进行中").arg(formatDateTimeShort(startTime))
                : QStringLiteral("%1 ~ %2").arg(formatDateTimeShort(startTime),
                                                formatDateTimeShort(endTime));
        }
        auto *chargeItem = new QTableWidgetItem(chargeTime);
        chargeItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnChargeTime, chargeItem);

        auto *kwhItem = new QTableWidgetItem(
            kwh > 0.0 ? QString::number(kwh, 'f', 2) : QStringLiteral("—"));
        kwhItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, kColumnKwh, kwhItem);

        auto *priceItem = new QTableWidgetItem(QStringLiteral("¥%1/度").arg(
            formatAmount(order.value(QStringLiteral("price_per_kwh")).toDouble())));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, kColumnPrice, priceItem);

        auto *feeItem = new QTableWidgetItem(
            settled ? QStringLiteral("¥%1").arg(formatAmount(fee)) : QStringLiteral("—"));
        feeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, kColumnFee, feeItem);
    }
    table_->setUpdatesEnabled(true);

    const bool filtered = !statusBox_->currentData().toString().isEmpty()
        || stationBox_->currentData().toInt() > 0
        || datePresetBox_->currentData().toInt() != kDateAll;
    setStatusText(filtered
        ? QStringLiteral("找到 %1 条匹配订单").arg(orders.size())
        : QStringLiteral("共 %1 条订单记录").arg(orders.size()), false);
}

void AdminOrderPage::resetFilters()
{
    const QSignalBlocker statusBlocker(statusBox_);
    const QSignalBlocker stationBlocker(stationBox_);
    const QSignalBlocker dateBlocker(datePresetBox_);
    statusBox_->setCurrentIndex(0);
    stationBox_->setCurrentIndex(0);
    datePresetBox_->setCurrentIndex(kDateAll);
    applyDatePreset();
    loadOrders();
}

void AdminOrderPage::setLoading(bool loading, const QString &message)
{
    if (loading) {
        loadingOverlay_->setText(message.isEmpty()
            ? QStringLiteral("加载中...") : message);
        loadingOverlay_->setGeometry(table_->rect());
        loadingOverlay_->raise();
        loadingOverlay_->show();
        table_->setEnabled(false);
        queryButton_->setEnabled(false);
        resetButton_->setEnabled(false);
    } else {
        loadingOverlay_->hide();
        table_->setEnabled(true);
        queryButton_->setEnabled(true);
        resetButton_->setEnabled(true);
    }
}

void AdminOrderPage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("tone", isError ? "error" : "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

} // namespace ev
