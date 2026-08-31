#include "mainwindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
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
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

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

QString cellText(QTableWidget *table, int row, int column)
{
    const QTableWidgetItem *item = row >= 0 ? table->item(row, column) : nullptr;
    return item ? item->text() : QString{};
}

QHBoxLayout *toolbar(QWidget *parent = nullptr)
{
    auto *layout = new QHBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    return layout;
}

void finishDialog(QDialog &dialog, QFormLayout *form)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电动汽车充电桩应用管理平台 · 运营管理端"));
    resize(1280, 820);

    stack_ = new QStackedWidget;
    stack_->setObjectName(QStringLiteral("adminStack"));
    loginPage_ = createLoginPage();
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("adminTabs"));
    tabs_->addTab(createDashboardPage(), QStringLiteral("经营概览"));
    tabs_->addTab(createStationPage(), QStringLiteral("站点"));
    tabs_->addTab(createChargerPage(), QStringLiteral("充电桩"));
    tabs_->addTab(createUserPage(), QStringLiteral("用户"));
    tabs_->addTab(createReservationPage(), QStringLiteral("预约"));
    tabs_->addTab(createSessionPage(), QStringLiteral("充电记录"));
    tabs_->addTab(createOrderPage(), QStringLiteral("订单"));
    tabs_->addTab(createTariffPage(), QStringLiteral("价格"));
    tabs_->addTab(createFaultPage(), QStringLiteral("故障"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"));
    tabs_->setCornerWidget(logoutButton, Qt::TopRightCorner);
    connect(logoutButton, &QPushButton::clicked, this, [this] {
        apiClient_.sendRequest(QStringLiteral("auth.logout"));
    });
    stack_->addWidget(loginPage_);
    stack_->addWidget(tabs_);
    setCentralWidget(stack_);
    connectSignals();
    QTimer::singleShot(0, this, &MainWindow::connectServer);
}

QWidget *MainWindow::createLoginPage()
{
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    outer->addStretch();
    auto *box = new QGroupBox(QStringLiteral("管理员登录"));
    box->setMaximumWidth(520);
    auto *form = new QFormLayout(box);
    connectionLabel_ = new QLabel(QStringLiteral("尚未连接"));
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"));
    portSpin_ = new QSpinBox;
    portSpin_->setObjectName(QStringLiteral("serverPort"));
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(45454);
    connectButton_ = new QPushButton(QStringLiteral("连接服务端"));
    connectButton_->setObjectName(QStringLiteral("connectButton"));
    auto *endpoint = new QWidget;
    auto *endpointLayout = toolbar(endpoint);
    endpointLayout->addWidget(hostEdit_, 2);
    endpointLayout->addWidget(portSpin_, 1);
    endpointLayout->addWidget(connectButton_);
    usernameEdit_ = new QLineEdit(QStringLiteral("admin"));
    passwordEdit_ = new QLineEdit(QStringLiteral("Admin123!"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    loginButton_ = new QPushButton(QStringLiteral("登录管理平台"));
    loginButton_->setObjectName(QStringLiteral("loginButton"));
    loginButton_->setDefault(true);
    form->addRow(QStringLiteral("连接状态"), connectionLabel_);
    form->addRow(QStringLiteral("服务端"), endpoint);
    form->addRow(QStringLiteral("用户名"), usernameEdit_);
    form->addRow(QStringLiteral("密码"), passwordEdit_);
    form->addRow(loginButton_);
    outer->addWidget(box, 0, Qt::AlignHCenter);
    outer->addStretch();
    return page;
}

QWidget *MainWindow::createDashboardPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *refresh = new QPushButton(QStringLiteral("刷新概览"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshDashboard);
    layout->addWidget(refresh, 0, Qt::AlignRight);
    auto *cards = new QGridLayout;
    userCountLabel_ = new QLabel;
    userCountLabel_->setObjectName(QStringLiteral("dashboardUserCount"));
    stationCountLabel_ = new QLabel;
    chargerCountLabel_ = new QLabel;
    chargerStateLabel_ = new QLabel;
    todayLabel_ = new QLabel;
    revenueLabel_ = new QLabel;
    const QList<QPair<QString, QLabel *>> entries{
        {QStringLiteral("用户数"), userCountLabel_},
        {QStringLiteral("运营站点"), stationCountLabel_},
        {QStringLiteral("充电桩总数"), chargerCountLabel_},
        {QStringLiteral("设备状态"), chargerStateLabel_},
        {QStringLiteral("今日经营"), todayLabel_},
        {QStringLiteral("营收汇总"), revenueLabel_}
    };
    for (int i = 0; i < entries.size(); ++i) {
        auto *box = new QGroupBox(entries.at(i).first);
        auto *boxLayout = new QVBoxLayout(box);
        entries.at(i).second->setText(QStringLiteral("--"));
        entries.at(i).second->setAlignment(Qt::AlignCenter);
        entries.at(i).second->setStyleSheet(QStringLiteral("font-size: 19px; font-weight: 600; padding: 14px;"));
        boxLayout->addWidget(entries.at(i).second);
        cards->addWidget(box, i / 3, i % 3);
    }
    layout->addLayout(cards);
    layout->addWidget(new QLabel(QStringLiteral("近 7 天订单与营收趋势")));
    trendTable_ = makeTable({QStringLiteral("日期"), QStringLiteral("订单数"), QStringLiteral("营收/元")});
    trendTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(trendTable_);
    return page;
}

QWidget *MainWindow::createStationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("新增站点"));
    auto *edit = new QPushButton(QStringLiteral("编辑选中"));
    bar->addWidget(refresh);
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshStations);
    connect(add, &QPushButton::clicked, this, [this] { editStation(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editStation(false); });
    layout->addLayout(bar);
    stationTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("区域"),
                               QStringLiteral("地址"), QStringLiteral("经度"), QStringLiteral("纬度"),
                               QStringLiteral("营业时间"), QStringLiteral("状态"), QStringLiteral("桩数"),
                               QStringLiteral("空闲"), QStringLiteral("最低价/元")});
    layout->addWidget(stationTable_);
    return page;
}

QWidget *MainWindow::createChargerPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("新增充电桩"));
    auto *edit = new QPushButton(QStringLiteral("编辑选中"));
    auto *status = new QPushButton(QStringLiteral("设置运行状态"));
    for (auto *button : {refresh, add, edit, status}) bar->addWidget(button);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshChargers);
    connect(add, &QPushButton::clicked, this, [this] { editCharger(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editCharger(false); });
    connect(status, &QPushButton::clicked, this, &MainWindow::setChargerStatus);
    layout->addLayout(bar);
    chargerTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("站点ID"), QStringLiteral("站点"),
                               QStringLiteral("编号"), QStringLiteral("接口"), QStringLiteral("功率/kW"),
                               QStringLiteral("状态"), QStringLiteral("价格ID"), QStringLiteral("价格方案"),
                               QStringLiteral("单价/元"), QStringLiteral("更新时间")});
    layout->addWidget(chargerTable_);
    return page;
}

QWidget *MainWindow::createUserPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *toggle = new QPushButton(QStringLiteral("启用/停用选中用户"));
    bar->addWidget(refresh);
    bar->addWidget(toggle);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshUsers);
    connect(toggle, &QPushButton::clicked, this, &MainWindow::setUserStatus);
    layout->addLayout(bar);
    userTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("用户名"), QStringLiteral("角色"),
                            QStringLiteral("姓名"), QStringLiteral("手机号"), QStringLiteral("余额/元"),
                            QStringLiteral("状态"), QStringLiteral("注册时间")});
    layout->addWidget(userTable_);
    return page;
}

QWidget *MainWindow::createOrderPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *refresh = new QPushButton(QStringLiteral("刷新订单"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshOrders);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    orderTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("订单号"), QStringLiteral("用户"),
                             QStringLiteral("站点"), QStringLiteral("充电桩"), QStringLiteral("电量/kWh"),
                             QStringLiteral("金额/元"), QStringLiteral("状态"), QStringLiteral("创建时间")});
    layout->addWidget(orderTable_);
    return page;
}

QWidget *MainWindow::createReservationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *refresh = new QPushButton(QStringLiteral("刷新预约"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshReservations);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    reservationTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("用户"), QStringLiteral("站点"),
                                   QStringLiteral("充电桩"), QStringLiteral("状态"), QStringLiteral("预约时间"),
                                   QStringLiteral("过期时间"), QStringLiteral("完成时间")});
    layout->addWidget(reservationTable_);
    return page;
}

QWidget *MainWindow::createSessionPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *refresh = new QPushButton(QStringLiteral("刷新充电记录"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshSessions);
    layout->addWidget(refresh, 0, Qt::AlignLeft);
    sessionTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("用户"), QStringLiteral("站点"),
                               QStringLiteral("充电桩"), QStringLiteral("状态"), QStringLiteral("开始时间"),
                               QStringLiteral("结束时间"), QStringLiteral("电量/kWh"), QStringLiteral("金额/元"),
                               QStringLiteral("单价/元")});
    layout->addWidget(sessionTable_);
    return page;
}

QWidget *MainWindow::createTariffPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("新增价格方案"));
    auto *edit = new QPushButton(QStringLiteral("编辑选中"));
    bar->addWidget(refresh);
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshTariffs);
    connect(add, &QPushButton::clicked, this, [this] { editTariff(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editTariff(false); });
    layout->addLayout(bar);
    tariffTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("方案名称"), QStringLiteral("单价/元/kWh"),
                              QStringLiteral("启用"), QStringLiteral("创建时间"), QStringLiteral("更新时间")});
    layout->addWidget(tariffTable_);
    return page;
}

QWidget *MainWindow::createFaultPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("登记故障"));
    auto *edit = new QPushButton(QStringLiteral("处理选中故障"));
    bar->addWidget(refresh);
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshFaults);
    connect(add, &QPushButton::clicked, this, [this] { editFault(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editFault(false); });
    layout->addLayout(bar);
    faultTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("充电桩ID"), QStringLiteral("充电桩"),
                             QStringLiteral("站点"), QStringLiteral("标题"), QStringLiteral("说明"),
                             QStringLiteral("状态"), QStringLiteral("上报时间"), QStringLiteral("解决时间")});
    layout->addWidget(faultTable_);
    return page;
}

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
void MainWindow::refreshChargers() { apiClient_.sendRequest(QStringLiteral("admin.charger.list")); }
void MainWindow::refreshUsers() { apiClient_.sendRequest(QStringLiteral("admin.user.list")); }
void MainWindow::refreshOrders() { apiClient_.sendRequest(QStringLiteral("admin.order.list")); }
void MainWindow::refreshReservations() { apiClient_.sendRequest(QStringLiteral("admin.reservation.list")); }
void MainWindow::refreshSessions() { apiClient_.sendRequest(QStringLiteral("admin.session.list")); }
void MainWindow::refreshTariffs() { apiClient_.sendRequest(QStringLiteral("admin.tariff.list")); }
void MainWindow::refreshFaults() { apiClient_.sendRequest(QStringLiteral("admin.fault.list")); }

qint64 MainWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    return row < 0 ? 0 : cellText(table, row, 0).toLongLong();
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
    status->addItems({QStringLiteral("active"), QStringLiteral("disabled")});
    if (!createNew) status->setCurrentText(cellText(stationTable_, row, 7));
    form->addRow(QStringLiteral("名称"), name); form->addRow(QStringLiteral("区域"), region);
    form->addRow(QStringLiteral("地址"), address); form->addRow(QStringLiteral("经度"), longitude);
    form->addRow(QStringLiteral("纬度"), latitude); form->addRow(QStringLiteral("营业时间"), hours);
    form->addRow(QStringLiteral("状态"), status); finishDialog(dialog, form);
    if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.station.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(stationTable_))},
        {QStringLiteral("name"), name->text()}, {QStringLiteral("region"), region->text()},
        {QStringLiteral("address"), address->text()}, {QStringLiteral("longitude"), longitude->value()},
        {QStringLiteral("latitude"), latitude->value()}, {QStringLiteral("businessHours"), hours->text()},
        {QStringLiteral("status"), status->currentText()}
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
    status->addItems({QStringLiteral("idle"), QStringLiteral("fault"), QStringLiteral("offline"), QStringLiteral("disabled")});
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
        {QStringLiteral("status"), status->currentText()}, {QStringLiteral("tariffId"), tariffId->value()}
    });
}

void MainWindow::setChargerStatus()
{
    const qint64 id = selectedId(chargerTable_);
    if (id <= 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个充电桩。")); return; }
    bool ok = false;
    const QString status = QInputDialog::getItem(this, QStringLiteral("设置运行状态"), QStringLiteral("目标状态"),
        {QStringLiteral("idle"), QStringLiteral("fault"), QStringLiteral("offline"), QStringLiteral("disabled")}, 0, false, &ok);
    if (ok) apiClient_.sendRequest(QStringLiteral("admin.charger.setStatus"), {
        {QStringLiteral("chargerId"), static_cast<double>(id)}, {QStringLiteral("status"), status}});
}

void MainWindow::setUserStatus()
{
    const qint64 id = selectedId(userTable_);
    const int row = userTable_->currentRow();
    if (id <= 0 || row < 0) { QMessageBox::information(this, QStringLiteral("请选择"), QStringLiteral("请先选择一个用户。")); return; }
    const QString target = cellText(userTable_, row, 6) == QStringLiteral("active")
                               ? QStringLiteral("disabled") : QStringLiteral("active");
    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定将用户 %1 设置为 %2？").arg(cellText(userTable_, row, 1), target)) != QMessageBox::Yes) return;
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
    auto *status = new QComboBox; status->addItems({QStringLiteral("open"), QStringLiteral("processing"), QStringLiteral("resolved")});
    if (!createNew) status->setCurrentText(cellText(faultTable_, row, 6));
    form->addRow(QStringLiteral("充电桩 ID"), chargerId); form->addRow(QStringLiteral("故障标题"), title);
    form->addRow(QStringLiteral("说明"), description); form->addRow(QStringLiteral("处理状态"), status);
    finishDialog(dialog, form); if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("admin.fault.save"), {
        {QStringLiteral("id"), createNew ? 0.0 : static_cast<double>(selectedId(faultTable_))},
        {QStringLiteral("chargerId"), chargerId->value()}, {QStringLiteral("title"), title->text()},
        {QStringLiteral("description"), description->text()}, {QStringLiteral("status"), status->currentText()}
    });
}

void MainWindow::handleResponse(const QString &action, bool ok, const QJsonObject &data,
                                const QString &errorCode, const QString &errorMessage)
{
    if (!ok) {
        statusBar()->showMessage(QStringLiteral("%1：%2").arg(errorCode, errorMessage), 8000);
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
        stack_->setCurrentWidget(tabs_);
        statusBar()->showMessage(QStringLiteral("管理员 %1 已登录").arg(user.value(QStringLiteral("displayName")).toString()), 5000);
        refreshAll();
    } else if (action == QStringLiteral("auth.logout")) {
        apiClient_.clearToken();
        stack_->setCurrentWidget(loginPage_);
        statusBar()->showMessage(QStringLiteral("已退出登录"), 5000);
    } else if (action == QStringLiteral("admin.dashboard")) populateDashboard(data);
    else if (action == QStringLiteral("admin.station.list")) populateStations(data);
    else if (action == QStringLiteral("admin.charger.list")) populateChargers(data);
    else if (action == QStringLiteral("admin.user.list")) populateUsers(data);
    else if (action == QStringLiteral("admin.order.list")) populateOrders(data);
    else if (action == QStringLiteral("admin.reservation.list")) populateReservations(data);
    else if (action == QStringLiteral("admin.session.list")) populateSessions(data);
    else if (action == QStringLiteral("admin.tariff.list")) populateTariffs(data);
    else if (action == QStringLiteral("admin.fault.list")) populateFaults(data);
    else if (action == QStringLiteral("admin.station.save")) { statusBar()->showMessage(QStringLiteral("站点已保存"), 4000); refreshStations(); refreshDashboard(); }
    else if (action == QStringLiteral("admin.charger.save") || action == QStringLiteral("admin.charger.setStatus")) { statusBar()->showMessage(QStringLiteral("充电桩已更新"), 4000); refreshChargers(); refreshDashboard(); }
    else if (action == QStringLiteral("admin.user.setStatus")) { statusBar()->showMessage(QStringLiteral("用户状态已更新"), 4000); refreshUsers(); }
    else if (action == QStringLiteral("admin.tariff.save")) { statusBar()->showMessage(QStringLiteral("价格方案已保存"), 4000); refreshTariffs(); refreshChargers(); }
    else if (action == QStringLiteral("admin.fault.save")) { statusBar()->showMessage(QStringLiteral("故障记录已保存"), 4000); refreshFaults(); refreshChargers(); refreshDashboard(); }
}

void MainWindow::populateDashboard(const QJsonObject &data)
{
    userCountLabel_->setText(QString::number(jsonInteger(data.value(QStringLiteral("userCount")))));
    stationCountLabel_->setText(QString::number(jsonInteger(data.value(QStringLiteral("stationCount")))));
    chargerCountLabel_->setText(QString::number(jsonInteger(data.value(QStringLiteral("chargerCount")))));
    chargerStateLabel_->setText(QStringLiteral("空闲 %1 / 充电 %2 / 故障 %3")
        .arg(jsonInteger(data.value(QStringLiteral("idleChargerCount"))))
        .arg(jsonInteger(data.value(QStringLiteral("chargingChargerCount"))))
        .arg(jsonInteger(data.value(QStringLiteral("faultChargerCount")))));
    todayLabel_->setText(QStringLiteral("%1 单 / ¥%2")
        .arg(jsonInteger(data.value(QStringLiteral("todayOrderCount"))))
        .arg(money(jsonInteger(data.value(QStringLiteral("todayRevenueCents"))))));
    revenueLabel_->setText(QStringLiteral("本月 ¥%1 / 累计 ¥%2")
        .arg(money(jsonInteger(data.value(QStringLiteral("monthRevenueCents")))))
        .arg(money(jsonInteger(data.value(QStringLiteral("totalRevenueCents"))))));
    const QJsonArray trend = data.value(QStringLiteral("sevenDayTrend")).toArray();
    trendTable_->setRowCount(trend.size());
    for (int row = 0; row < trend.size(); ++row) {
        const QJsonObject item = trend.at(row).toObject();
        setCell(trendTable_, row, 0, item.value(QStringLiteral("date")).toString());
        setCell(trendTable_, row, 1, QString::number(jsonInteger(item.value(QStringLiteral("orderCount")))));
        setCell(trendTable_, row, 2, money(jsonInteger(item.value(QStringLiteral("revenueCents")))));
    }
}

void MainWindow::populateStations(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("stations")).toArray(); stationTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("name").toString(), x.value("region").toString(),
            x.value("address").toString(), QString::number(x.value("longitude").toDouble(), 'f', 6), QString::number(x.value("latitude").toDouble(), 'f', 6),
            x.value("businessHours").toString(), x.value("status").toString(), QString::number(jsonInteger(x.value("chargerCount"))),
            QString::number(jsonInteger(x.value("idleCount"))), money(jsonInteger(x.value("minimumPriceCentsPerKwh")))};
        for (int column = 0; column < values.size(); ++column) setCell(stationTable_, row, column, values.at(column));
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
            x.value("status").toString(), QString::number(jsonInteger(x.value("tariffId"))), x.value("tariffName").toString(),
            money(jsonInteger(x.value("priceCentsPerKwh"))), x.value("updatedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(chargerTable_, row, column, values.at(column));
    }
    chargerTable_->resizeColumnsToContents();
}

void MainWindow::populateUsers(const QJsonObject &data)
{
    const QJsonArray items = data.value(QStringLiteral("users")).toArray(); userTable_->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const QJsonObject x = items.at(row).toObject();
        const QStringList values{QString::number(jsonInteger(x.value("id"))), x.value("username").toString(), x.value("role").toString(),
            x.value("displayName").toString(), x.value("phone").toString(), money(jsonInteger(x.value("balanceCents"))),
            x.value("status").toString(), x.value("createdAt").toString()};
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
            x.value("stationName").toString(), x.value("chargerCode").toString(), QString::number(jsonInteger(x.value("energyWh")) / 1000.0, 'f', 3),
            money(jsonInteger(x.value("amountCents"))), x.value("status").toString(), x.value("createdAt").toString()};
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
            x.value("stationName").toString(), x.value("chargerCode").toString(), x.value("status").toString(),
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
            x.value("stationName").toString(), x.value("chargerCode").toString(), x.value("status").toString(),
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
            x.value("description").toString(), x.value("status").toString(), x.value("reportedAt").toString(), x.value("resolvedAt").toString()};
        for (int column = 0; column < values.size(); ++column) setCell(faultTable_, row, column, values.at(column));
    }
    faultTable_->resizeColumnsToContents();
}

} // namespace evcs::adminclient
