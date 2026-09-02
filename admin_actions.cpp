#include "admin_mainwindow.h"
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
#include <QHash>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
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
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace evcs::adminclient {
namespace {

QString cellText(QTableWidget *table, int row, int column)
{
    const QTableWidgetItem *item = row >= 0 ? table->item(row, column) : nullptr;
    return item ? item->text() : QString{};
}

void finishDialog(QDialog &dialog, QFormLayout *form)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
}

} // namespace

// 本文件处理管理操作、筛选条件和服务端请求。
void MainWindow::connectSignals()
{
    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectServer);
    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(&apiClient_, &ApiClient::connectionChanged, this,
            [this](bool connected, const QString &message) {
                connectionLabel_->setText(message);
                loginButton_->setEnabled(connected);
                statusBar()->showMessage(message, 5000);
                if (!connected) {
                    apiClient_.clearToken();
                    stack_->setCurrentWidget(loginPage_);
                }
            });
    connect(&apiClient_, &ApiClient::responseReceived, this,
            [this](const QString &, const QString &action, bool ok, const QJsonObject &data,
                   const QString &code, const QString &message) {
                handleResponse(action, ok, data, code, message);
            });
}

void MainWindow::connectServer()
{
    apiClient_.connectToServer(hostEdit_->text().trimmed(), static_cast<quint16>(portSpin_->value()));
}

void MainWindow::login()
{
    if (!apiClient_.isConnected()) {
        QMessageBox::warning(this, QStringLiteral("尚未连接"), QStringLiteral("请先连接服务端。"));
        return;
    }
    apiClient_.sendRequest(QStringLiteral("auth.login"), {
        {QStringLiteral("username"), usernameEdit_->text().trimmed()},
        {QStringLiteral("password"), passwordEdit_->text()}
    });
}

void MainWindow::refreshAll()
{
    refreshDashboard();
    refreshStations();
    refreshChargers();
    refreshUsers();
    refreshOrders();
    refreshReservations();
    refreshSessions();
    refreshTariffs();
    refreshFaults();
}

void MainWindow::refreshDashboard() { apiClient_.sendRequest(QStringLiteral("admin.dashboard")); }
void MainWindow::refreshStations() { apiClient_.sendRequest(QStringLiteral("admin.station.list")); }
void MainWindow::refreshChargers()
{
    QJsonObject payload;
    if (chargerStationFilterId_ > 0) payload.insert(QStringLiteral("stationId"), static_cast<double>(chargerStationFilterId_));
    apiClient_.sendRequest(QStringLiteral("admin.charger.list"), payload);
}

void MainWindow::refreshUsers()
{
    apiClient_.sendRequest(QStringLiteral("admin.user.list"), {
        {QStringLiteral("phoneKeyword"), userPhoneFilterEdit_->text().trimmed()}
    });
}

void MainWindow::refreshOrders()
{
    QJsonObject payload{
        {QStringLiteral("orderNo"), orderNoFilterEdit_->text().trimmed()},
        {QStringLiteral("phone"), orderPhoneFilterEdit_->text().trimmed()},
        {QStringLiteral("stationKeyword"), orderStationFilterEdit_->text().trimmed()},
        {QStringLiteral("chargerCode"), orderChargerFilterEdit_->text().trimmed()}
    };
    if (orderStatusFilterCombo_->currentIndex() > 0)
        payload.insert(QStringLiteral("status"), orderStatusFilterCombo_->currentData().toString());
    if (orderDateFilterCheck_->isChecked()) {
        payload.insert(QStringLiteral("startDate"), orderStartDateEdit_->date().toString(Qt::ISODate));
        payload.insert(QStringLiteral("endDate"), orderEndDateEdit_->date().toString(Qt::ISODate));
    }
    apiClient_.sendRequest(QStringLiteral("admin.order.list"), payload);
}
void MainWindow::refreshReservations() { apiClient_.sendRequest(QStringLiteral("admin.reservation.list")); }
void MainWindow::refreshSessions() { apiClient_.sendRequest(QStringLiteral("admin.session.list")); }
void MainWindow::refreshTariffs() { apiClient_.sendRequest(QStringLiteral("admin.tariff.list")); }
void MainWindow::refreshFaults() { apiClient_.sendRequest(QStringLiteral("admin.fault.list")); }

qint64 MainWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    return row < 0 ? 0 : cellText(table, row, 0).toLongLong();
}

void MainWindow::showSelectedStationDevices()
{
    const int row = stationTable_->currentRow();
    const qint64 stationId = selectedId(stationTable_);
    if (row < 0 || stationId <= 0) {
        QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个站点。"));
        return;
    }
    chargerStationFilterId_ = stationId;
    chargerFilterLabel_->setText(QStringLiteral("当前站点：%1（ID %2）")
                                     .arg(cellText(stationTable_, row, 1)).arg(stationId));
    tabs_->setCurrentIndex(2);
    refreshChargers();
}

void MainWindow::showAllChargers()
{
    chargerStationFilterId_ = 0;
    chargerFilterLabel_->setText(QStringLiteral("当前：全部站点"));
    refreshChargers();
}

void MainWindow::editStation(bool createNew)
{
    const int row = stationTable_->currentRow();
    if (!createNew && row < 0) {
        QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个站点。"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(createNew ? QStringLiteral("新增站点") : QStringLiteral("编辑站点"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(createNew ? QString{} : cellText(stationTable_, row, 1));
    auto *region = new QLineEdit(createNew ? QStringLiteral("北京市") : cellText(stationTable_, row, 2));
    auto *address = new QLineEdit(createNew ? QString{} : cellText(stationTable_, row, 3));
    auto *longitude = new QDoubleSpinBox;
    longitude->setRange(-180, 180); longitude->setDecimals(6);
    longitude->setValue(createNew ? 116.397 : cellText(stationTable_, row, 4).toDouble());
    auto *latitude = new QDoubleSpinBox;
    latitude->setRange(-90, 90); latitude->setDecimals(6);
    latitude->setValue(createNew ? 39.908 : cellText(stationTable_, row, 5).toDouble());
    auto *hours = new QLineEdit(createNew ? QStringLiteral("00:00-24:00") : cellText(stationTable_, row, 6));
    auto *status = new QComboBox;
    status->addItem(QStringLiteral("正常"), QStringLiteral("active"));
    status->addItem(QStringLiteral("停用"), QStringLiteral("disabled"));
    if (!createNew) status->setCurrentText(cellText(stationTable_, row, 7));
    auto *chargerCount = new QSpinBox;
    chargerCount->setRange(0, 50);
    chargerCount->setValue(createNew ? 4 : 0);
    chargerCount->setEnabled(createNew);
    auto *defaultPower = new QDoubleSpinBox;
    defaultPower->setRange(1, 1000); defaultPower->setDecimals(1); defaultPower->setValue(60.0);
    defaultPower->setEnabled(createNew);
    auto *tariffId = new QSpinBox;
    tariffId->setRange(1, 100000000); tariffId->setValue(1); tariffId->setEnabled(createNew);
    form->addRow(QStringLiteral("名称"), name); form->addRow(QStringLiteral("区域"), region);
    form->addRow(QStringLiteral("地址"), address); form->addRow(QStringLiteral("经度"), longitude);
    form->addRow(QStringLiteral("纬度"), latitude); form->addRow(QStringLiteral("营业时间"), hours);
    form->addRow(QStringLiteral("状态"), status);
    form->addRow(QStringLiteral("同步创建充电桩"), chargerCount);
    form->addRow(QStringLiteral("默认功率/kW"), defaultPower);
    form->addRow(QStringLiteral("价格方案 ID"), tariffId);
    finishDialog(dialog, form);
    if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.station.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(stationTable_))},
        {QStringLiteral("name"), name->text()}, {QStringLiteral("region"), region->text()},
        {QStringLiteral("address"), address->text()}, {QStringLiteral("longitude"), longitude->value()},
        {QStringLiteral("latitude"), latitude->value()}, {QStringLiteral("businessHours"), hours->text()},
        {QStringLiteral("status"), status->currentData().toString()},
        {QStringLiteral("chargerCount"), chargerCount->value()},
        {QStringLiteral("defaultPowerKw"), defaultPower->value()},
        {QStringLiteral("tariffId"), tariffId->value()}
    });
}

void MainWindow::editCharger(bool createNew)
{
    const int row = chargerTable_->currentRow();
    if (!createNew && row < 0) {
        QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个充电桩。"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(createNew ? QStringLiteral("新增充电桩") : QStringLiteral("编辑充电桩"));
    auto *form = new QFormLayout(&dialog);
    auto *stationId = new QSpinBox; stationId->setRange(1, 100000000);
    stationId->setValue(createNew ? 1 : cellText(chargerTable_, row, 1).toInt());
    auto *code = new QLineEdit(createNew ? QString{} : cellText(chargerTable_, row, 3));
    auto *connector = new QLineEdit(createNew ? QStringLiteral("GB/T") : cellText(chargerTable_, row, 4));
    auto *power = new QDoubleSpinBox; power->setRange(1, 1000); power->setDecimals(1);
    power->setValue(createNew ? 60 : cellText(chargerTable_, row, 5).toDouble());
    auto *status = new QComboBox;
    status->addItem(QStringLiteral("空闲"), QStringLiteral("idle"));
    status->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    status->addItem(QStringLiteral("离线"), QStringLiteral("offline"));
    status->addItem(QStringLiteral("停用"), QStringLiteral("disabled"));
    if (!createNew) {
        const QString current = cellText(chargerTable_, row, 6);
        if (status->findText(current) < 0) status->addItem(current);
        status->setCurrentText(current);
    }
    auto *tariffId = new QSpinBox; tariffId->setRange(1, 100000000);
    tariffId->setValue(createNew ? 1 : cellText(chargerTable_, row, 7).toInt());
    form->addRow(QStringLiteral("站点 ID"), stationId); form->addRow(QStringLiteral("设备编号"), code);
    form->addRow(QStringLiteral("接口标准"), connector); form->addRow(QStringLiteral("额定功率/kW"), power);
    form->addRow(QStringLiteral("状态"), status); form->addRow(QStringLiteral("价格方案 ID"), tariffId);
    finishDialog(dialog, form);
    if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.charger.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(chargerTable_))},
        {QStringLiteral("stationId"), stationId->value()}, {QStringLiteral("code"), code->text()},
        {QStringLiteral("connectorType"), connector->text()}, {QStringLiteral("ratedPowerKw"), power->value()},
        {QStringLiteral("status"), status->currentData().toString()}, {QStringLiteral("tariffId"), tariffId->value()}
    });
}

void MainWindow::setChargerStatus()
{
    const qint64 id = selectedId(chargerTable_);
    if (id <= 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个充电桩。")); return; }
    bool ok = false;
    const QString statusLabel = QInputDialog::getItem(this, QStringLiteral("设置运行状态"), QStringLiteral("目标状态"),
        {QStringLiteral("空闲"), QStringLiteral("故障"), QStringLiteral("离线"), QStringLiteral("停用")}, 0, false, &ok);
    const QHash<QString, QString> statusCodes{
        {QStringLiteral("空闲"), QStringLiteral("idle")},
        {QStringLiteral("故障"), QStringLiteral("fault")},
        {QStringLiteral("离线"), QStringLiteral("offline")},
        {QStringLiteral("停用"), QStringLiteral("disabled")}
    };
    if (ok) apiClient_.sendRequest(QStringLiteral("admin.charger.setStatus"), {
        {QStringLiteral("chargerId"), static_cast<double>(id)},
        {QStringLiteral("status"), statusCodes.value(statusLabel)}});
}

void MainWindow::restartCharger()
{
    const qint64 id = selectedId(chargerTable_);
    if (id <= 0) {
        QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个充电桩。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("远程重启"),
            QStringLiteral("确认向充电桩 %1 发送重启指令？").arg(cellText(chargerTable_, chargerTable_->currentRow(), 3)))
        != QMessageBox::Yes) return;
    apiClient_.sendRequest(QStringLiteral("admin.charger.restart"), {
        {QStringLiteral("chargerId"), static_cast<double>(id)}
    });
}

void MainWindow::showChargerOperations()
{
    QJsonObject payload;
    const qint64 id = selectedId(chargerTable_);
    if (id > 0) payload.insert(QStringLiteral("chargerId"), static_cast<double>(id));
    apiClient_.sendRequest(QStringLiteral("admin.charger.operation.list"), payload);
}

void MainWindow::setUserStatus()
{
    const qint64 id = selectedId(userTable_);
    const int row = userTable_->currentRow();
    if (id <= 0 || row < 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个用户。")); return; }
    const QString target = cellText(userTable_, row, 6) == QStringLiteral("正常")
                               ? QStringLiteral("disabled") : QStringLiteral("active");
    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定将用户 %1 设置为%2？")
                .arg(cellText(userTable_, row, 1), statusText(target))) != QMessageBox::Yes) return;
    apiClient_.sendRequest(QStringLiteral("admin.user.setStatus"), {
        {QStringLiteral("userId"), static_cast<double>(id)}, {QStringLiteral("status"), target}});
}

void MainWindow::editTariff(bool createNew)
{
    const int row = tariffTable_->currentRow();
    if (!createNew && row < 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择价格方案。")); return; }
    QDialog dialog(this); dialog.setWindowTitle(createNew ? QStringLiteral("新增价格方案") : QStringLiteral("编辑价格方案"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(createNew ? QString{} : cellText(tariffTable_, row, 1));
    auto *price = new QDoubleSpinBox; price->setRange(0, 100); price->setDecimals(2); price->setSuffix(QStringLiteral(" 元/kWh"));
    price->setValue(createNew ? 1.20 : cellText(tariffTable_, row, 2).toDouble());
    auto *active = new QCheckBox(QStringLiteral("启用")); active->setChecked(createNew || cellText(tariffTable_, row, 3) == QStringLiteral("是"));
    form->addRow(QStringLiteral("名称"), name); form->addRow(QStringLiteral("单价"), price); form->addRow(active);
    finishDialog(dialog, form); if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.tariff.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(tariffTable_))},
        {QStringLiteral("name"), name->text()},
        {QStringLiteral("priceCentsPerKwh"), qRound(price->value() * 100.0)},
        {QStringLiteral("active"), active->isChecked()}
    });
}

void MainWindow::editFault(bool createNew)
{
    const int row = faultTable_->currentRow();
    if (!createNew && row < 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择故障记录。")); return; }
    QDialog dialog(this); dialog.setWindowTitle(createNew ? QStringLiteral("登记故障") : QStringLiteral("处理故障"));
    auto *form = new QFormLayout(&dialog);
    auto *chargerId = new QSpinBox; chargerId->setRange(1, 100000000);
    chargerId->setValue(createNew ? 1 : cellText(faultTable_, row, 1).toInt()); chargerId->setEnabled(createNew);
    auto *title = new QLineEdit(createNew ? QString{} : cellText(faultTable_, row, 4));
    auto *description = new QLineEdit(createNew ? QString{} : cellText(faultTable_, row, 5));
    auto *status = new QComboBox;
    status->addItem(QStringLiteral("待处理"), QStringLiteral("open"));
    status->addItem(QStringLiteral("处理中"), QStringLiteral("processing"));
    status->addItem(QStringLiteral("已解决"), QStringLiteral("resolved"));
    if (!createNew) status->setCurrentText(cellText(faultTable_, row, 6));
    form->addRow(QStringLiteral("充电桩 ID"), chargerId); form->addRow(QStringLiteral("故障标题"), title);
    form->addRow(QStringLiteral("说明"), description); form->addRow(QStringLiteral("处理状态"), status);
    finishDialog(dialog, form); if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.fault.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(faultTable_))},
        {QStringLiteral("chargerId"), chargerId->value()}, {QStringLiteral("title"), title->text()},
        {QStringLiteral("description"), description->text()},
        {QStringLiteral("status"), status->currentData().toString()}
    });
}


} // namespace evcs::adminclient
