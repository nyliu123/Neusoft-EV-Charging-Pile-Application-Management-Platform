#include "mainwindow.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QVBoxLayout>

namespace evcs::userclient {

namespace {

void configureTable(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
}

QTableWidgetItem *makeIdItem(qint64 id)
{
    auto *item = new QTableWidgetItem(QString::number(id));
    item->setData(Qt::UserRole, id);
    return item;
}

QString moneyText(qint64 cents)
{
    return QStringLiteral("¥%1").arg(static_cast<double>(cents) / 100.0, 0, 'f', 2);
}

QString localTimeText(const QString &isoText)
{
    if (isoText.isEmpty()) return {};
    const QDateTime time = QDateTime::fromString(isoText, Qt::ISODateWithMs);
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : isoText;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电动汽车充电用户端"));
    resize(760, 780);

    stack_ = new QStackedWidget;
    stack_->setObjectName(QStringLiteral("userStack"));
    loginPage_ = createLoginPage();
    stack_->addWidget(loginPage_);

    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("userTabs"));
    tabs_->addTab(createStationPage(), QStringLiteral("附近站点"));
    tabs_->addTab(createReservationPage(), QStringLiteral("我的预约"));
    tabs_->addTab(createChargingPage(), QStringLiteral("正在充电"));
    tabs_->addTab(createOrderPage(), QStringLiteral("历史订单"));
    tabs_->addTab(createProfilePage(), QStringLiteral("个人中心"));
    stack_->addWidget(tabs_);
    setCentralWidget(stack_);

    connectSignals();
    chargingTimer_.setInterval(1000);
    connect(&chargingTimer_, &QTimer::timeout, this, &MainWindow::refreshChargingStatus);
    apiClient_.connectToServer(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
}

QWidget *MainWindow::createLoginPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(80, 60, 80, 60);

    auto *title = new QLabel(QStringLiteral("<h1>充电用户端</h1><p>找桩、预约、充电和订单查询</p>"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *connectionBox = new QGroupBox(QStringLiteral("服务端连接"));
    auto *connectionForm = new QFormLayout(connectionBox);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"));
    portSpin_ = new QSpinBox;
    portSpin_->setObjectName(QStringLiteral("serverPort"));
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(45454);
    connectButton_ = new QPushButton(QStringLiteral("重新连接"));
    connectButton_->setObjectName(QStringLiteral("connectButton"));
    connectionLabel_ = new QLabel(QStringLiteral("未连接"));
    connectionForm->addRow(QStringLiteral("主机"), hostEdit_);
    connectionForm->addRow(QStringLiteral("端口"), portSpin_);
    connectionForm->addRow(connectButton_, connectionLabel_);
    layout->addWidget(connectionBox);

    auto *loginBox = new QGroupBox(QStringLiteral("用户登录"));
    auto *loginForm = new QFormLayout(loginBox);
    usernameEdit_ = new QLineEdit(QStringLiteral("demo"));
    passwordEdit_ = new QLineEdit(QStringLiteral("Demo123!"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    loginButton_ = new QPushButton(QStringLiteral("登录"));
    loginButton_->setObjectName(QStringLiteral("loginButton"));
    auto *registerButton = new QPushButton(QStringLiteral("注册新用户"));
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(loginButton_);
    buttons->addWidget(registerButton);
    loginForm->addRow(QStringLiteral("用户名"), usernameEdit_);
    loginForm->addRow(QStringLiteral("密码"), passwordEdit_);
    loginForm->addRow(buttons);
    layout->addWidget(loginBox);
    layout->addStretch();

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectServer);
    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(registerButton, &QPushButton::clicked, this, &MainWindow::registerUser);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &MainWindow::login);
    return page;
}

QWidget *MainWindow::createStationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *search = new QHBoxLayout;
    stationKeywordEdit_ = new QLineEdit;
    stationKeywordEdit_->setPlaceholderText(QStringLiteral("站点名称或地址"));
    stationRegionEdit_ = new QLineEdit;
    stationRegionEdit_->setPlaceholderText(QStringLiteral("区域（可选）"));
    onlyAvailableCheck_ = new QCheckBox(QStringLiteral("仅显示有空闲桩"));
    onlyAvailableCheck_->setChecked(true);
    auto *refresh = new QPushButton(QStringLiteral("查询"));
    search->addWidget(stationKeywordEdit_);
    search->addWidget(stationRegionEdit_);
    search->addWidget(onlyAvailableCheck_);
    search->addWidget(refresh);
    layout->addLayout(search);

    stationTable_ = new QTableWidget;
    stationTable_->setObjectName(QStringLiteral("stationTable"));
    configureTable(stationTable_, {QStringLiteral("编号"), QStringLiteral("名称"),
                                   QStringLiteral("区域"), QStringLiteral("地址"),
                                   QStringLiteral("空闲/全部"), QStringLiteral("最低价格")});
    layout->addWidget(stationTable_, 3);
    auto *loadStation = new QPushButton(QStringLiteral("查看所选站点的充电桩"));
    layout->addWidget(loadStation);

    chargerTable_ = new QTableWidget;
    configureTable(chargerTable_, {QStringLiteral("编号"), QStringLiteral("桩编号"),
                                   QStringLiteral("功率"), QStringLiteral("状态"),
                                   QStringLiteral("价格")});
    layout->addWidget(chargerTable_, 2);
    auto *chargerButtons = new QHBoxLayout;
    auto *reserve = new QPushButton(QStringLiteral("预约所选充电桩"));
    auto *start = new QPushButton(QStringLiteral("直接开始充电"));
    chargerButtons->addWidget(reserve);
    chargerButtons->addWidget(start);
    layout->addLayout(chargerButtons);

    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshStations);
    connect(loadStation, &QPushButton::clicked, this, &MainWindow::loadSelectedStation);
    connect(stationTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { loadSelectedStation(); });
    connect(reserve, &QPushButton::clicked, this, &MainWindow::reserveSelectedCharger);
    connect(start, &QPushButton::clicked, this, &MainWindow::startSelectedCharger);
    return page;
}

QWidget *MainWindow::createReservationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *buttons = new QHBoxLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *cancel = new QPushButton(QStringLiteral("取消预约"));
    auto *start = new QPushButton(QStringLiteral("使用预约开始充电"));
    buttons->addWidget(refresh);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(start);
    layout->addLayout(buttons);
    reservationTable_ = new QTableWidget;
    configureTable(reservationTable_, {QStringLiteral("编号"), QStringLiteral("站点"),
                                       QStringLiteral("充电桩"), QStringLiteral("状态"),
                                       QStringLiteral("预约时间"), QStringLiteral("过期时间")});
    layout->addWidget(reservationTable_);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshReservations);
    connect(cancel, &QPushButton::clicked, this, &MainWindow::cancelSelectedReservation);
    connect(start, &QPushButton::clicked, this, &MainWindow::startSelectedReservation);
    return page;
}

QWidget *MainWindow::createChargingPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(80, 80, 80, 80);
    auto *title = new QLabel(QStringLiteral("<h1>实时充电</h1>"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *form = new QFormLayout;
    chargingStationLabel_ = new QLabel(QStringLiteral("暂无"));
    chargingChargerLabel_ = new QLabel(QStringLiteral("暂无"));
    chargingTimeLabel_ = new QLabel(QStringLiteral("0 秒"));
    chargingEnergyLabel_ = new QLabel(QStringLiteral("0.000 kWh"));
    chargingAmountLabel_ = new QLabel(QStringLiteral("¥0.00"));
    form->addRow(QStringLiteral("站点"), chargingStationLabel_);
    form->addRow(QStringLiteral("充电桩"), chargingChargerLabel_);
    form->addRow(QStringLiteral("已充时间"), chargingTimeLabel_);
    form->addRow(QStringLiteral("已充电量"), chargingEnergyLabel_);
    form->addRow(QStringLiteral("预估金额"), chargingAmountLabel_);
    layout->addLayout(form);
    stopChargingButton_ = new QPushButton(QStringLiteral("结束充电并结算"));
    stopChargingButton_->setEnabled(false);
    layout->addWidget(stopChargingButton_);
    layout->addStretch();
    connect(stopChargingButton_, &QPushButton::clicked, this, &MainWindow::stopCharging);
    return page;
}

QWidget *MainWindow::createOrderPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *buttons = new QHBoxLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新订单"));
    auto *detail = new QPushButton(QStringLiteral("查看订单明细"));
    buttons->addWidget(refresh);
    buttons->addWidget(detail);
    buttons->addStretch();
    layout->addLayout(buttons);
    orderTable_ = new QTableWidget;
    configureTable(orderTable_, {QStringLiteral("编号"), QStringLiteral("订单号"),
                                 QStringLiteral("站点"), QStringLiteral("充电桩"),
                                 QStringLiteral("电量"), QStringLiteral("金额"),
                                 QStringLiteral("状态"), QStringLiteral("时间")});
    layout->addWidget(orderTable_);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshOrders);
    connect(detail, &QPushButton::clicked, this, &MainWindow::showSelectedOrder);
    connect(orderTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { showSelectedOrder(); });
    return page;
}

QWidget *MainWindow::createProfilePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(100, 80, 100, 80);
    auto *box = new QGroupBox(QStringLiteral("个人信息"));
    auto *form = new QFormLayout(box);
    profileUsernameLabel_ = new QLabel(QStringLiteral("--"));
    profileDisplayNameLabel_ = new QLabel(QStringLiteral("--"));
    profilePhoneLabel_ = new QLabel(QStringLiteral("--"));
    profileBalanceLabel_ = new QLabel(QStringLiteral("--"));
    profileCreatedAtLabel_ = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("用户名"), profileUsernameLabel_);
    form->addRow(QStringLiteral("姓名"), profileDisplayNameLabel_);
    form->addRow(QStringLiteral("手机号"), profilePhoneLabel_);
    form->addRow(QStringLiteral("账户余额"), profileBalanceLabel_);
    form->addRow(QStringLiteral("注册时间"), profileCreatedAtLabel_);
    layout->addWidget(box);
    auto *buttons = new QHBoxLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新个人信息"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"));
    buttons->addWidget(refresh);
    buttons->addStretch();
    buttons->addWidget(logoutButton);
    layout->addLayout(buttons);
    layout->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshProfile);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    return page;
}

void MainWindow::connectSignals()
{
    connect(&apiClient_, &ApiClient::connectionChanged, this,
            [this](bool connected, const QString &message) {
        connectionLabel_->setText(message);
        loginButton_->setEnabled(connected);
        statusBar()->showMessage(message);
        if (!connected) {
            chargingTimer_.stop();
            apiClient_.clearToken();
            stack_->setCurrentWidget(loginPage_);
        }
    });
    connect(&apiClient_, &ApiClient::responseReceived, this,
            [this](const QString &, const QString &action, bool ok, const QJsonObject &data,
                   const QString &errorCode, const QString &errorMessage) {
        handleResponse(action, ok, data, errorCode, errorMessage);
    });
}

void MainWindow::connectServer()
{
    apiClient_.connectToServer(hostEdit_->text().trimmed(),
                               static_cast<quint16>(portSpin_->value()));
}

void MainWindow::login()
{
    if (!apiClient_.isConnected()) {
        QMessageBox::warning(this, QStringLiteral("未连接"), QStringLiteral("请先连接服务端"));
        return;
    }
    apiClient_.sendRequest(QStringLiteral("auth.login"), {
        {QStringLiteral("username"), usernameEdit_->text().trimmed()},
        {QStringLiteral("password"), passwordEdit_->text()}
    });
}

void MainWindow::registerUser()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("注册用户"));
    QFormLayout form(&dialog);
    QLineEdit username;
    QLineEdit password;
    password.setEchoMode(QLineEdit::Password);
    QLineEdit displayName;
    QLineEdit phone;
    form.addRow(QStringLiteral("用户名"), &username);
    form.addRow(QStringLiteral("密码"), &password);
    form.addRow(QStringLiteral("姓名"), &displayName);
    form.addRow(QStringLiteral("手机号"), &phone);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("auth.register"), {
        {QStringLiteral("username"), username.text().trimmed()},
        {QStringLiteral("password"), password.text()},
        {QStringLiteral("displayName"), displayName.text().trimmed()},
        {QStringLiteral("phone"), phone.text().trimmed()}
    });
}

void MainWindow::refreshStations()
{
    apiClient_.sendRequest(QStringLiteral("station.list"), {
        {QStringLiteral("keyword"), stationKeywordEdit_->text().trimmed()},
        {QStringLiteral("region"), stationRegionEdit_->text().trimmed()},
        {QStringLiteral("onlyAvailable"), onlyAvailableCheck_->isChecked()}
    });
}

void MainWindow::loadSelectedStation()
{
    const qint64 stationId = selectedId(stationTable_);
    if (stationId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("station.get"),
                           {{QStringLiteral("stationId"), static_cast<double>(stationId)}});
}

void MainWindow::reserveSelectedCharger()
{
    const qint64 chargerId = selectedId(chargerTable_);
    if (chargerId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("reservation.create"),
                           {{QStringLiteral("chargerId"), static_cast<double>(chargerId)}});
}

void MainWindow::startSelectedCharger()
{
    const qint64 chargerId = selectedId(chargerTable_);
    if (chargerId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("charging.start"),
                           {{QStringLiteral("chargerId"), static_cast<double>(chargerId)}});
}

void MainWindow::refreshReservations()
{
    apiClient_.sendRequest(QStringLiteral("reservation.list"));
}

void MainWindow::cancelSelectedReservation()
{
    const qint64 reservationId = selectedId(reservationTable_);
    if (reservationId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("reservation.cancel"),
                           {{QStringLiteral("reservationId"), static_cast<double>(reservationId)}});
}

void MainWindow::startSelectedReservation()
{
    const qint64 reservationId = selectedId(reservationTable_);
    if (reservationId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("charging.start"),
                           {{QStringLiteral("reservationId"), static_cast<double>(reservationId)}});
}

void MainWindow::refreshChargingStatus()
{
    QJsonObject payload;
    if (activeSessionId_ > 0) {
        payload.insert(QStringLiteral("sessionId"), static_cast<double>(activeSessionId_));
    }
    apiClient_.sendRequest(QStringLiteral("charging.status"), payload);
}

void MainWindow::stopCharging()
{
    if (activeSessionId_ <= 0) return;
    apiClient_.sendRequest(QStringLiteral("charging.stop"),
                           {{QStringLiteral("sessionId"), static_cast<double>(activeSessionId_)}});
}

void MainWindow::refreshOrders()
{
    apiClient_.sendRequest(QStringLiteral("order.list"));
}

void MainWindow::showSelectedOrder()
{
    const qint64 orderId = selectedId(orderTable_);
    if (orderId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("order.get"),
                           {{QStringLiteral("orderId"), static_cast<double>(orderId)}});
}

void MainWindow::refreshProfile()
{
    apiClient_.sendRequest(QStringLiteral("user.profile"));
}

void MainWindow::logout()
{
    chargingTimer_.stop();
    apiClient_.sendRequest(QStringLiteral("auth.logout"));
}

qint64 MainWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0)) {
        QMessageBox::information(const_cast<MainWindow *>(this), QStringLiteral("请选择"),
                                 QStringLiteral("请先选择一行"));
        return 0;
    }
    return table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void MainWindow::handleResponse(const QString &action,
                                bool ok,
                                const QJsonObject &data,
                                const QString &errorCode,
                                const QString &errorMessage)
{
    if (!ok) {
        if (action == QStringLiteral("charging.status") && errorCode == QStringLiteral("NOT_FOUND")) {
            chargingTimer_.stop();
            return;
        }
        QMessageBox::warning(this, QStringLiteral("操作失败"),
                             QStringLiteral("%1\n%2").arg(errorCode, errorMessage));
        return;
    }

    if (action == QStringLiteral("auth.login")) {
        apiClient_.setToken(data.value(QStringLiteral("token")).toString());
        const QJsonObject user = data.value(QStringLiteral("user")).toObject();
        if (user.value(QStringLiteral("role")).toString() != QStringLiteral("user")) {
            apiClient_.clearToken();
            QMessageBox::warning(this, QStringLiteral("角色不匹配"),
                                 QStringLiteral("请使用普通用户账号登录用户端"));
            return;
        }
        statusBar()->showMessage(QStringLiteral("当前用户：%1，余额 %2")
            .arg(user.value(QStringLiteral("displayName")).toString(),
                 moneyText(static_cast<qint64>(user.value(QStringLiteral("balanceCents")).toDouble()))));
        stack_->setCurrentWidget(tabs_);
        refreshStations();
        refreshReservations();
        refreshOrders();
        refreshProfile();
        refreshChargingStatus();
    } else if (action == QStringLiteral("auth.register")) {
        QMessageBox::information(this, QStringLiteral("注册成功"),
                                 QStringLiteral("用户已创建，可以登录"));
    } else if (action == QStringLiteral("station.list")) {
        populateStations(data);
    } else if (action == QStringLiteral("station.get")) {
        populateChargers(data);
    } else if (action == QStringLiteral("reservation.create")) {
        QMessageBox::information(this, QStringLiteral("预约成功"),
                                 QStringLiteral("预约有效期 15 分钟"));
        refreshReservations();
        refreshStations();
    } else if (action == QStringLiteral("reservation.cancel")) {
        refreshReservations();
        refreshStations();
    } else if (action == QStringLiteral("reservation.list")) {
        populateReservations(data);
    } else if (action == QStringLiteral("charging.start")) {
        activeSessionId_ = static_cast<qint64>(data.value(QStringLiteral("sessionId")).toDouble());
        tabs_->setCurrentIndex(2);
        stopChargingButton_->setEnabled(true);
        chargingTimer_.start();
        refreshChargingStatus();
        refreshReservations();
    } else if (action == QStringLiteral("charging.status")) {
        populateCharging(data);
    } else if (action == QStringLiteral("charging.stop")) {
        chargingTimer_.stop();
        activeSessionId_ = 0;
        stopChargingButton_->setEnabled(false);
        const QJsonObject order = data.value(QStringLiteral("order")).toObject();
        QMessageBox::information(this, QStringLiteral("结算完成"),
            QStringLiteral("订单 %1\n电量 %2 kWh\n金额 %3\n状态 %4")
                .arg(order.value(QStringLiteral("orderNo")).toString())
                .arg(order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble())))
                .arg(order.value(QStringLiteral("status")).toString()));
        tabs_->setCurrentIndex(3);
        refreshOrders();
        refreshStations();
    } else if (action == QStringLiteral("order.list")) {
        populateOrders(data);
    } else if (action == QStringLiteral("order.get")) {
        const QJsonObject order = data.value(QStringLiteral("order")).toObject();
        QMessageBox::information(this, QStringLiteral("订单明细"),
            QStringLiteral("订单号：%1\n站点：%2\n地址：%3\n充电桩：%4\n"
                           "开始：%5\n结束：%6\n电量：%7 kWh\n单价：%8/kWh\n金额：%9\n状态：%10")
                .arg(order.value(QStringLiteral("orderNo")).toString(),
                     order.value(QStringLiteral("stationName")).toString(),
                     order.value(QStringLiteral("stationAddress")).toString(),
                     order.value(QStringLiteral("chargerCode")).toString(),
                     localTimeText(order.value(QStringLiteral("startedAt")).toString()),
                     localTimeText(order.value(QStringLiteral("endedAt")).toString()))
                .arg(order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("priceCentsPerKwh")).toDouble())))
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble())))
                .arg(order.value(QStringLiteral("status")).toString()));
    } else if (action == QStringLiteral("user.profile")) {
        populateProfile(data);
    } else if (action == QStringLiteral("auth.logout")) {
        apiClient_.clearToken();
        activeSessionId_ = 0;
        stopChargingButton_->setEnabled(false);
        stack_->setCurrentWidget(loginPage_);
        statusBar()->showMessage(QStringLiteral("已退出登录"), 5000);
    }
}

void MainWindow::populateStations(const QJsonObject &data)
{
    const QJsonArray stations = data.value(QStringLiteral("stations")).toArray();
    stationTable_->setRowCount(stations.size());
    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations.at(row).toObject();
        const qint64 id = static_cast<qint64>(station.value(QStringLiteral("id")).toDouble());
        stationTable_->setItem(row, 0, makeIdItem(id));
        stationTable_->setItem(row, 1, new QTableWidgetItem(station.value(QStringLiteral("name")).toString()));
        stationTable_->setItem(row, 2, new QTableWidgetItem(station.value(QStringLiteral("region")).toString()));
        stationTable_->setItem(row, 3, new QTableWidgetItem(station.value(QStringLiteral("address")).toString()));
        stationTable_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1/%2")
            .arg(station.value(QStringLiteral("idleCount")).toInt())
            .arg(station.value(QStringLiteral("chargerCount")).toInt())));
        stationTable_->setItem(row, 5, new QTableWidgetItem(
            moneyText(station.value(QStringLiteral("minimumPriceCentsPerKwh")).toInt())
            + QStringLiteral("/kWh")));
    }
    if (!stations.isEmpty()) stationTable_->selectRow(0);
}

void MainWindow::populateChargers(const QJsonObject &data)
{
    const QJsonArray chargers = data.value(QStringLiteral("station")).toObject()
                                    .value(QStringLiteral("chargers")).toArray();
    chargerTable_->setRowCount(chargers.size());
    for (int row = 0; row < chargers.size(); ++row) {
        const QJsonObject charger = chargers.at(row).toObject();
        const qint64 id = static_cast<qint64>(charger.value(QStringLiteral("id")).toDouble());
        chargerTable_->setItem(row, 0, makeIdItem(id));
        chargerTable_->setItem(row, 1, new QTableWidgetItem(charger.value(QStringLiteral("code")).toString()));
        chargerTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 kW").arg(
            charger.value(QStringLiteral("ratedPowerKw")).toDouble(), 0, 'f', 1)));
        chargerTable_->setItem(row, 3, new QTableWidgetItem(charger.value(QStringLiteral("status")).toString()));
        chargerTable_->setItem(row, 4, new QTableWidgetItem(
            moneyText(charger.value(QStringLiteral("priceCentsPerKwh")).toInt())
            + QStringLiteral("/kWh")));
    }
    if (!chargers.isEmpty()) chargerTable_->selectRow(0);
}

void MainWindow::populateReservations(const QJsonObject &data)
{
    const QJsonArray reservations = data.value(QStringLiteral("reservations")).toArray();
    reservationTable_->setRowCount(reservations.size());
    for (int row = 0; row < reservations.size(); ++row) {
        const QJsonObject reservation = reservations.at(row).toObject();
        const qint64 id = static_cast<qint64>(reservation.value(QStringLiteral("id")).toDouble());
        reservationTable_->setItem(row, 0, makeIdItem(id));
        reservationTable_->setItem(row, 1, new QTableWidgetItem(reservation.value(QStringLiteral("stationName")).toString()));
        reservationTable_->setItem(row, 2, new QTableWidgetItem(reservation.value(QStringLiteral("chargerCode")).toString()));
        reservationTable_->setItem(row, 3, new QTableWidgetItem(reservation.value(QStringLiteral("status")).toString()));
        reservationTable_->setItem(row, 4, new QTableWidgetItem(localTimeText(reservation.value(QStringLiteral("reservedAt")).toString())));
        reservationTable_->setItem(row, 5, new QTableWidgetItem(localTimeText(reservation.value(QStringLiteral("expiresAt")).toString())));
    }
    if (!reservations.isEmpty()) reservationTable_->selectRow(0);
}

void MainWindow::populateCharging(const QJsonObject &data)
{
    const QJsonObject session = data.value(QStringLiteral("session")).toObject();
    activeSessionId_ = static_cast<qint64>(session.value(QStringLiteral("id")).toDouble());
    chargingStationLabel_->setText(session.value(QStringLiteral("stationName")).toString());
    chargingChargerLabel_->setText(session.value(QStringLiteral("chargerCode")).toString());
    chargingTimeLabel_->setText(QStringLiteral("%1 秒").arg(
        static_cast<qint64>(session.value(QStringLiteral("elapsedSeconds")).toDouble())));
    chargingEnergyLabel_->setText(QStringLiteral("%1 kWh").arg(
        session.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3));
    chargingAmountLabel_->setText(moneyText(
        static_cast<qint64>(session.value(QStringLiteral("amountCents")).toDouble())));
    const bool charging = session.value(QStringLiteral("status")).toString() == QStringLiteral("charging");
    stopChargingButton_->setEnabled(charging);
    if (charging && !chargingTimer_.isActive()) chargingTimer_.start();
}

void MainWindow::populateOrders(const QJsonObject &data)
{
    const QJsonArray orders = data.value(QStringLiteral("orders")).toArray();
    orderTable_->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const QJsonObject order = orders.at(row).toObject();
        const qint64 id = static_cast<qint64>(order.value(QStringLiteral("id")).toDouble());
        orderTable_->setItem(row, 0, makeIdItem(id));
        orderTable_->setItem(row, 1, new QTableWidgetItem(order.value(QStringLiteral("orderNo")).toString()));
        orderTable_->setItem(row, 2, new QTableWidgetItem(order.value(QStringLiteral("stationName")).toString()));
        orderTable_->setItem(row, 3, new QTableWidgetItem(order.value(QStringLiteral("chargerCode")).toString()));
        orderTable_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1 kWh").arg(
            order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)));
        orderTable_->setItem(row, 5, new QTableWidgetItem(moneyText(
            static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble()))));
        orderTable_->setItem(row, 6, new QTableWidgetItem(order.value(QStringLiteral("status")).toString()));
        orderTable_->setItem(row, 7, new QTableWidgetItem(localTimeText(order.value(QStringLiteral("createdAt")).toString())));
    }
}

void MainWindow::populateProfile(const QJsonObject &data)
{
    const QJsonObject user = data.value(QStringLiteral("user")).toObject();
    profileUsernameLabel_->setText(user.value(QStringLiteral("username")).toString());
    profileDisplayNameLabel_->setText(user.value(QStringLiteral("displayName")).toString());
    profilePhoneLabel_->setText(user.value(QStringLiteral("phone")).toString());
    profileBalanceLabel_->setText(moneyText(
        static_cast<qint64>(user.value(QStringLiteral("balanceCents")).toDouble())));
    profileCreatedAtLabel_->setText(localTimeText(user.value(QStringLiteral("createdAt")).toString()));
}

} // namespace evcs::userclient
