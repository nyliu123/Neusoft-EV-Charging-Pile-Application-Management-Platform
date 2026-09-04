#include "admin_mainwindow.h"
#include "admin_session.h"
#include "ui_text.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QPieSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace evcs::adminclient {
namespace {

qint64 jsonInteger(const QJsonValue &value)
{
    return static_cast<qint64>(value.toDouble());
}

QString money(qint64 cents)
{
    return QString::number(cents / 100.0, 'f', 2);
}

QTableWidget *makeTable(const QStringList &headers)
{
    auto *table = new QTableWidget;
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

void setCell(QTableWidget *table, int row, int column, const QString &text)
{
    table->setItem(row, column, new QTableWidgetItem(text));
}

} // namespace

// 本文件统一处理管理端响应，并刷新表格、指标和图表。
void MainWindow::handleResponse(const QString &action, bool ok, const QJsonObject &data,
                                const QString &errorCode, const QString &errorMessage)
{
    if (!ok) {
        Q_UNUSED(errorCode);
        statusBar()->showMessage(QStringLiteral("操作失败：%1").arg(errorMessage), 8000);
        QMessageBox::warning(this, QStringLiteral("操作失败"), errorMessage);
        return;
    }
    if (action == QStringLiteral("auth.login")) {
        const QJsonObject user = data.value(QStringLiteral("user")).toObject();
        if (user.value(QStringLiteral("role")).toString() != QStringLiteral("admin")) {
            apiClient_.clearToken();
            QMessageBox::warning(this, QStringLiteral("无管理权限"), QStringLiteral("该账号不是管理员。"));
            return;
        }
        apiClient_.setToken(data.value(QStringLiteral("token")).toString());
        AdminSession::instance().setAuthenticated(apiClient_.token(), user);
        stack_->setCurrentWidget(tabs_);
        statusBar()->showMessage(QStringLiteral("管理员 %1 已登录").arg(user.value(QStringLiteral("displayName")).toString()), 5000);
        refreshAll();
    } else if (action == QStringLiteral("auth.logout")) {
        apiClient_.clearToken();
        AdminSession::instance().clear();
        stack_->setCurrentWidget(loginPage_);
        statusBar()->showMessage(QStringLiteral("已退出登录"), 5000);
    } else if (action == QStringLiteral("admin.dashboard")) populateDashboard(data);
    else if (action == QStringLiteral("admin.station.list")) populateStations(data);
    else if (action == QStringLiteral("map.geocode")) {
        if (pendingLongitude_ && pendingLatitude_) {
            pendingLongitude_->setValue(data.value(QStringLiteral("longitude")).toDouble());
            pendingLatitude_->setValue(data.value(QStringLiteral("latitude")).toDouble());
            statusBar()->showMessage(QStringLiteral("地址解析成功：%1")
                .arg(data.value(QStringLiteral("title")).toString()), 5000);
        }
    }
    else if (action == QStringLiteral("admin.charger.list")) populateChargers(data);
    else if (action == QStringLiteral("admin.user.list")) populateUsers(data);
    else if (action == QStringLiteral("admin.order.list")) populateOrders(data);
    else if (action == QStringLiteral("admin.reservation.list")) populateReservations(data);
    else if (action == QStringLiteral("admin.session.list")) populateSessions(data);
    else if (action == QStringLiteral("admin.tariff.list")) populateTariffs(data);
    else if (action == QStringLiteral("admin.fault.list")) populateFaults(data);
    else if (action == QStringLiteral("admin.charger.operation.list")) {
        const QJsonArray items = data.value(QStringLiteral("operations")).toArray();
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("充电桩远程操作日志"));
        dialog.resize(1040, 480);
        auto *layout = new QVBoxLayout(&dialog);
        auto *table = makeTable({QStringLiteral("ID"), QStringLiteral("充电桩"), QStringLiteral("管理员"),
                                 QStringLiteral("操作"), QStringLiteral("原状态"), QStringLiteral("结果状态"),
                                 QStringLiteral("成功"), QStringLiteral("说明"), QStringLiteral("时间")});
        table->setRowCount(items.size());
        for (int row = 0; row < items.size(); ++row) {
            const QJsonObject x = items.at(row).toObject();
            const QStringList values{
                QString::number(jsonInteger(x.value(QStringLiteral("id")))),
                x.value(QStringLiteral("chargerCode")).toString(),
                x.value(QStringLiteral("operator")).toString(),
                x.value(QStringLiteral("operation")).toString() == QStringLiteral("restart")
                    ? QStringLiteral("远程重启") : x.value(QStringLiteral("operation")).toString(),
                statusText(x.value(QStringLiteral("previousStatus")).toString()),
                statusText(x.value(QStringLiteral("resultStatus")).toString()),
                x.value(QStringLiteral("success")).toBool() ? QStringLiteral("是") : QStringLiteral("否"),
                x.value(QStringLiteral("message")).toString(),
                x.value(QStringLiteral("createdAt")).toString()
            };
            for (int column = 0; column < values.size(); ++column) setCell(table, row, column, values.at(column));
        }
        table->resizeColumnsToContents();
        layout->addWidget(table);
        auto *close = new QPushButton(QStringLiteral("关闭"));
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        layout->addWidget(close, 0, Qt::AlignRight);
        dialog.exec();
    }
    else if (action == QStringLiteral("admin.station.save")) { statusBar()->showMessage(QStringLiteral("站点已保存"), 4000); refreshStations(); refreshDashboard(); }
    else if (action == QStringLiteral("admin.charger.save") || action == QStringLiteral("admin.charger.setStatus")) { statusBar()->showMessage(QStringLiteral("充电桩已更新"), 4000); refreshChargers(); refreshDashboard(); }
    else if (action == QStringLiteral("admin.charger.restart")) {
        QMessageBox::information(this, QStringLiteral("远程重启"), data.value(QStringLiteral("message")).toString());
        refreshChargers(); refreshFaults(); refreshDashboard();
    }
    else if (action == QStringLiteral("admin.user.setStatus")) { statusBar()->showMessage(QStringLiteral("用户状态已更新"), 4000); refreshUsers(); }
    else if (action == QStringLiteral("admin.tariff.save")) { statusBar()->showMessage(QStringLiteral("价格方案已保存"), 4000); refreshTariffs(); refreshChargers(); }
    else if (action == QStringLiteral("admin.fault.save")) { statusBar()->showMessage(QStringLiteral("故障记录已保存"), 4000); refreshFaults(); refreshChargers(); refreshDashboard(); }
}

void MainWindow::populateDashboard(const QJsonObject &data)
{
    dashboardData_ = data;
    userCountLabel_->setText(QString::number(jsonInteger(data.value(QStringLiteral("userCount")))));
    stationCountLabel_->setText(QStringLiteral("%1 kWh").arg(
        jsonInteger(data.value(QStringLiteral("totalEnergyWh"))) / 1000.0, 0, 'f', 3));
    chargerCountLabel_->setText(QString::number(jsonInteger(data.value(QStringLiteral("totalOrderCount")))));
    chargerStateLabel_->setText(QStringLiteral("空闲 %1 / 预约 %2 / 充电 %3 / 故障 %4")
        .arg(jsonInteger(data.value(QStringLiteral("idleChargerCount"))))
        .arg(jsonInteger(data.value(QStringLiteral("reservedChargerCount"))))
        .arg(jsonInteger(data.value(QStringLiteral("chargingChargerCount"))))
        .arg(jsonInteger(data.value(QStringLiteral("faultChargerCount")))));
    todayLabel_->setText(QStringLiteral("%1 单 / ¥%2")
        .arg(jsonInteger(data.value(QStringLiteral("todayOrderCount"))))
        .arg(money(jsonInteger(data.value(QStringLiteral("todayRevenueCents"))))));
    revenueLabel_->setText(QStringLiteral("¥%1")
        .arg(money(jsonInteger(data.value(QStringLiteral("totalRevenueCents"))))));
    auto *pie = new QPieSeries;
    pie->append(QStringLiteral("空闲"), jsonInteger(data.value(QStringLiteral("idleChargerCount"))));
    pie->append(QStringLiteral("已预约"), jsonInteger(data.value(QStringLiteral("reservedChargerCount"))));
    pie->append(QStringLiteral("充电中"), jsonInteger(data.value(QStringLiteral("chargingChargerCount"))));
    pie->append(QStringLiteral("故障"), jsonInteger(data.value(QStringLiteral("faultChargerCount"))));
    auto *statusChart = new QChart;
    statusChart->setTitle(QStringLiteral("全量充电桩状态统计"));
    statusChart->addSeries(pie);
    statusChart->legend()->setVisible(true);
    QChart *oldStatusChart = statusChart_->chart();
    statusChart_->setChart(statusChart);
    delete oldStatusChart;
    updateTrendDisplay();
}

void MainWindow::updateTrendDisplay()
{
    const int days = trendRangeCombo_ ? trendRangeCombo_->currentData().toInt() : 7;
    const QJsonArray trend = dashboardData_.value(days == 30
            ? QStringLiteral("thirtyDayTrend") : QStringLiteral("sevenDayTrend")).toArray();
    trendTable_->setRowCount(trend.size());
    auto *orderSeries = new QLineSeries;
    auto *revenueSeries = new QLineSeries;
    orderSeries->setName(QStringLiteral("订单数"));
    revenueSeries->setName(QStringLiteral("营收/元"));
    qreal maximumOrders = 1;
    qreal maximumRevenue = 1;
    for (int row = 0; row < trend.size(); ++row) {
        const QJsonObject item = trend.at(row).toObject();
        const qreal orderCount = jsonInteger(item.value(QStringLiteral("orderCount")));
        const qreal revenue = jsonInteger(item.value(QStringLiteral("revenueCents"))) / 100.0;
        const QDate date = QDate::fromString(item.value(QStringLiteral("date")).toString(), Qt::ISODate);
        const qreal timestamp = QDateTime(date.startOfDay()).toMSecsSinceEpoch();
        orderSeries->append(timestamp, orderCount);
        revenueSeries->append(timestamp, revenue);
        maximumOrders = qMax(maximumOrders, orderCount);
        maximumRevenue = qMax(maximumRevenue, revenue);
        setCell(trendTable_, row, 0, item.value(QStringLiteral("date")).toString());
        setCell(trendTable_, row, 1, QString::number(jsonInteger(item.value(QStringLiteral("orderCount")))));
        setCell(trendTable_, row, 2, money(jsonInteger(item.value(QStringLiteral("revenueCents")))));
    }
    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("近 %1 天订单与营收趋势").arg(days));
    chart->addSeries(orderSeries);
    chart->addSeries(revenueSeries);
    auto *dateAxis = new QDateTimeAxis;
    dateAxis->setFormat(days == 30 ? QStringLiteral("MM-dd") : QStringLiteral("MM-dd"));
    dateAxis->setTickCount(days == 30 ? 7 : 7);
    if (!trend.isEmpty()) {
        const QDate firstDate = QDate::fromString(
            trend.first().toObject().value(QStringLiteral("date")).toString(), Qt::ISODate);
        const QDate lastDate = QDate::fromString(
            trend.last().toObject().value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (firstDate.isValid() && lastDate.isValid()) {
            dateAxis->setRange(firstDate.startOfDay(), lastDate.endOfDay());
        }
    }
    chart->addAxis(dateAxis, Qt::AlignBottom);
    orderSeries->attachAxis(dateAxis);
    revenueSeries->attachAxis(dateAxis);
    auto *orderAxis = new QValueAxis;
    orderAxis->setTitleText(QStringLiteral("订单数"));
    orderAxis->setRange(0, qCeil(maximumOrders * 1.2));
    orderAxis->setLabelFormat(QStringLiteral("%.0f"));
    chart->addAxis(orderAxis, Qt::AlignLeft);
    orderSeries->attachAxis(orderAxis);
    auto *revenueAxis = new QValueAxis;
    revenueAxis->setTitleText(QStringLiteral("营收/元"));
    revenueAxis->setRange(0, maximumRevenue * 1.2);
    revenueAxis->setLabelFormat(QStringLiteral("%.2f"));
    chart->addAxis(revenueAxis, Qt::AlignRight);
    revenueSeries->attachAxis(revenueAxis);
    QChart *oldChart = trendChart_->chart();
    trendChart_->setChart(chart);
    delete oldChart;
}

void MainWindow::populateStations(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("stations")).toArray(); stationTable_->setRowCount(items.size());
    const qint64 selectedChargerStation = chargerStationFilterCombo_
        ? chargerStationFilterCombo_->currentData().toLongLong() : 0;
    const qint64 selectedOrderStation = orderStationFilterCombo_
        ? orderStationFilterCombo_->currentData().toLongLong() : 0;
    const QSignalBlocker chargerBlocker(chargerStationFilterCombo_);
    const QSignalBlocker orderBlocker(orderStationFilterCombo_);
    if (chargerStationFilterCombo_) {
        chargerStationFilterCombo_->clear();
        chargerStationFilterCombo_->addItem(QStringLiteral("全部站点"), 0);
    }
    if (orderStationFilterCombo_) {
        orderStationFilterCombo_->clear();
        orderStationFilterCombo_->addItem(QStringLiteral("全部站点"), 0);
    }
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const qint64 stationId = jsonInteger(x.value("id"));
        const QString stationName = x.value("name").toString();
        if (chargerStationFilterCombo_) chargerStationFilterCombo_->addItem(stationName, stationId);
        if (orderStationFilterCombo_) orderStationFilterCombo_->addItem(stationName, stationId);
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("name").toString(), x.value("region").toString(),
            x.value("address").toString(), QString::number(x.value("longitude").toDouble(), 'f', 6), QString::number(x.value("latitude").toDouble(), 'f', 6),
            x.value("businessHours").toString(), statusText(x.value("status").toString()), QString::number(jsonInteger(x.value("chargerCount"))),
            QString::number(jsonInteger(x.value("idleCount"))),
            QStringLiteral("%1%").arg(x.value("onlineRate").toDouble() * 100.0, 0, 'f', 1),
            money(jsonInteger(x.value("minimumPriceCentsPerKwh")))};
        for (int column = 0; column < values.size(); ++column) setCell(stationTable_, row, column, values.at(column));
    }
    if (chargerStationFilterCombo_) {
        const int index = chargerStationFilterCombo_->findData(selectedChargerStation);
        chargerStationFilterCombo_->setCurrentIndex(index < 0 ? 0 : index);
    }
    if (orderStationFilterCombo_) {
        const int index = orderStationFilterCombo_->findData(selectedOrderStation);
        orderStationFilterCombo_->setCurrentIndex(index < 0 ? 0 : index);
    }
    stationTable_->resizeColumnsToContents();
}

void MainWindow::populateChargers(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("chargers")).toArray(); chargerTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), QString::number(jsonInteger(x.value("stationId"))), x.value("stationName").toString(),
            x.value("code").toString(), x.value("connectorType").toString(), QString::number(x.value("ratedPowerKw").toDouble(), 'f', 1),
            statusText(x.value("status").toString()), QString::number(jsonInteger(x.value("tariffId"))), x.value("tariffName").toString(),
            money(jsonInteger(x.value("priceCentsPerKwh"))),
            QString::number(jsonInteger(x.value("sessionCount"))),
            QString::number(jsonInteger(x.value("totalDurationSeconds")) / 60.0, 'f', 1),
            x.value("updatedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(chargerTable_, row, column, values.at(column));
    }
    chargerTable_->resizeColumnsToContents();
}

void MainWindow::populateUsers(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("users")).toArray(); userTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("username").toString(), roleText(x.value("role").toString()),
            x.value("displayName").toString(), x.value("phone").toString(), money(jsonInteger(x.value("balanceCents"))),
            statusText(x.value("status").toString()), x.value("createdAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(userTable_, row, column, values.at(column));
    }
    userTable_->resizeColumnsToContents();
}

void MainWindow::populateOrders(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("orders")).toArray(); orderTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("orderNo").toString(), x.value("username").toString(),
            x.value("phone").toString(), x.value("stationName").toString(), x.value("chargerCode").toString(), QString::number(jsonInteger(x.value("energyWh")) / 1000.0, 'f', 3),
            money(jsonInteger(x.value("amountCents"))), statusText(x.value("status").toString()), x.value("createdAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(orderTable_, row, column, values.at(column));
    }
    orderTable_->resizeColumnsToContents();
}

void MainWindow::populateReservations(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("reservations")).toArray();
    reservationTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("username").toString(),
            x.value("stationName").toString(), x.value("chargerCode").toString(), statusText(x.value("status").toString()),
            x.value("reservedAt").toString(), x.value("expiresAt").toString(), x.value("completedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(reservationTable_, row, column, values.at(column));
    }
    reservationTable_->resizeColumnsToContents();
}

void MainWindow::populateSessions(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("sessions")).toArray();
    sessionTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("username").toString(),
            x.value("stationName").toString(), x.value("chargerCode").toString(), statusText(x.value("status").toString()),
            x.value("startedAt").toString(), x.value("endedAt").toString(),
            QString::number(jsonInteger(x.value("energyWh")) / 1000.0, 'f', 3),
            money(jsonInteger(x.value("estimatedAmountCents"))), money(jsonInteger(x.value("priceCentsPerKwh")))};
        for (int column = 0; column < values.size(); ++column) setCell(sessionTable_, row, column, values.at(column));
    }
    sessionTable_->resizeColumnsToContents();
}

void MainWindow::populateTariffs(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("tariffs")).toArray(); tariffTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("name").toString(),
            money(jsonInteger(x.value("priceCentsPerKwh"))), x.value("active").toBool() ? QStringLiteral("是") : QStringLiteral("否"),
            x.value("createdAt").toString(), x.value("updatedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(tariffTable_, row, column, values.at(column));
    }
    tariffTable_->resizeColumnsToContents();
}

void MainWindow::populateFaults(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("faults")).toArray(); faultTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), QString::number(jsonInteger(x.value("chargerId"))),
            x.value("chargerCode").toString(), x.value("stationName").toString(), x.value("title").toString(),
            x.value("description").toString(), statusText(x.value("status").toString()), x.value("reportedAt").toString(), x.value("resolvedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(faultTable_, row, column, values.at(column));
    }
    faultTable_->resizeColumnsToContents();
}

} // namespace evcs::adminclient
