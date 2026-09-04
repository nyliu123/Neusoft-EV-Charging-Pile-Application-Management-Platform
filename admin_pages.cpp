#include "admin_mainwindow.h"

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
#include <QtCharts/QPieSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace evcs::adminclient {
namespace {

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

QHBoxLayout *toolbar(QWidget *parent = nullptr)
{
    auto *layout = new QHBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    return layout;
}

} // namespace

// 本文件集中构造管理端页面；每个页面使用独立 QSS 资源。
QWidget *MainWindow::createLoginPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("adminLoginPage"));
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
    portSpin_->setValue(8888);
    connectButton_ = new QPushButton(QStringLiteral("连接服务端"));
    connectButton_->setObjectName(QStringLiteral("connectButton"));
    auto *endpoint = new QWidget;
    auto *endpointLayout = toolbar(endpoint);
    endpointLayout->addWidget(hostEdit_, 2);
    endpointLayout->addWidget(portSpin_, 1);
    endpointLayout->addWidget(connectButton_);
    usernameEdit_ = new QLineEdit(QStringLiteral("admin"));
    passwordEdit_ = new QLineEdit;
    passwordEdit_->setObjectName(QStringLiteral("adminPassword"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入管理员密码"));
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
    page->setObjectName(QStringLiteral("adminDashboardPage"));
    auto *layout = new QVBoxLayout(page);
    auto *topBar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新概览"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshDashboard);
    trendRangeCombo_ = new QComboBox;
    trendRangeCombo_->addItem(QStringLiteral("近 7 天"), 7);
    trendRangeCombo_->addItem(QStringLiteral("近 30 天"), 30);
    connect(trendRangeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateTrendDisplay);
    topBar->addWidget(new QLabel(QStringLiteral("趋势范围")));
    topBar->addWidget(trendRangeCombo_);
    topBar->addStretch();
    topBar->addWidget(refresh);
    layout->addLayout(topBar);
    auto *cards = new QGridLayout;
    userCountLabel_ = new QLabel;
    userCountLabel_->setObjectName(QStringLiteral("dashboardUserCount"));
    stationCountLabel_ = new QLabel;
    chargerCountLabel_ = new QLabel;
    chargerStateLabel_ = new QLabel;
    todayLabel_ = new QLabel;
    revenueLabel_ = new QLabel;
    const QList<QPair<QString, QLabel *>> entries{
        {QStringLiteral("注册用户数"), userCountLabel_},
        {QStringLiteral("累计充电量"), stationCountLabel_},
        {QStringLiteral("累计充电次数"), chargerCountLabel_},
        {QStringLiteral("设备状态"), chargerStateLabel_},
        {QStringLiteral("今日经营"), todayLabel_},
        {QStringLiteral("累计营收"), revenueLabel_}
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
    layout->addWidget(new QLabel(QStringLiteral("订单与营收趋势（Qt Charts）")));
    trendChart_ = new QChartView;
    trendChart_->setMinimumHeight(250);
    trendChart_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(trendChart_);
    layout->addWidget(new QLabel(QStringLiteral("充电桩状态分布（Qt Charts）")));
    statusChart_ = new QChartView;
    statusChart_->setMinimumHeight(220);
    statusChart_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(statusChart_);
    trendTable_ = makeTable({QStringLiteral("日期"), QStringLiteral("订单数"), QStringLiteral("营收/元")});
    trendTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    trendTable_->setMaximumHeight(190);
    layout->addWidget(trendTable_);
    return page;
}

QWidget *MainWindow::createStationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("adminStationPage"));
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("新增站点"));
    auto *edit = new QPushButton(QStringLiteral("编辑选中"));
    auto *devices = new QPushButton(QStringLiteral("查看站内设备"));
    bar->addWidget(refresh);
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addWidget(devices);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshStations);
    connect(add, &QPushButton::clicked, this, [this] { editStation(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editStation(false); });
    connect(devices, &QPushButton::clicked, this, &MainWindow::showSelectedStationDevices);
    layout->addLayout(bar);
    stationTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("区域"),
                               QStringLiteral("地址"), QStringLiteral("经度"), QStringLiteral("纬度"),
                               QStringLiteral("营业时间"), QStringLiteral("状态"), QStringLiteral("桩数"),
                               QStringLiteral("空闲"), QStringLiteral("在线率"), QStringLiteral("最低价/元")});
    connect(stationTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { showSelectedStationDevices(); });
    layout->addWidget(stationTable_);
    return page;
}

QWidget *MainWindow::createChargerPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("adminChargerPage"));
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    auto *add = new QPushButton(QStringLiteral("新增充电桩"));
    auto *edit = new QPushButton(QStringLiteral("编辑选中"));
    auto *status = new QPushButton(QStringLiteral("设置运行状态"));
    auto *restart = new QPushButton(QStringLiteral("远程重启"));
    auto *operations = new QPushButton(QStringLiteral("操作日志"));
    auto *showAll = new QPushButton(QStringLiteral("显示全部"));
    chargerStationFilterCombo_ = new QComboBox;
    chargerStationFilterCombo_->addItem(QStringLiteral("全部站点"), 0);
    chargerStatusFilterCombo_ = new QComboBox;
    chargerStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString{});
    chargerStatusFilterCombo_->addItem(QStringLiteral("空闲"), QStringLiteral("idle"));
    chargerStatusFilterCombo_->addItem(QStringLiteral("已预约"), QStringLiteral("reserved"));
    chargerStatusFilterCombo_->addItem(QStringLiteral("充电中"), QStringLiteral("charging"));
    chargerStatusFilterCombo_->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    for (auto *button : {refresh, add, edit, status, restart, operations, showAll}) bar->addWidget(button);
    bar->addWidget(chargerStationFilterCombo_);
    bar->addWidget(chargerStatusFilterCombo_);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshChargers);
    connect(add, &QPushButton::clicked, this, [this] { editCharger(true); });
    connect(edit, &QPushButton::clicked, this, [this] { editCharger(false); });
    connect(status, &QPushButton::clicked, this, &MainWindow::setChargerStatus);
    connect(restart, &QPushButton::clicked, this, &MainWindow::restartCharger);
    connect(operations, &QPushButton::clicked, this, &MainWindow::showChargerOperations);
    connect(showAll, &QPushButton::clicked, this, &MainWindow::showAllChargers);
    connect(chargerStationFilterCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshChargers);
    connect(chargerStatusFilterCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshChargers);
    chargerFilterLabel_ = new QLabel(QStringLiteral("当前：全部站点"));
    bar->addWidget(chargerFilterLabel_);
    layout->addLayout(bar);
    chargerTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("站点ID"), QStringLiteral("站点"),
                               QStringLiteral("编号"), QStringLiteral("接口"), QStringLiteral("功率/kW"),
                               QStringLiteral("状态"), QStringLiteral("价格ID"), QStringLiteral("价格方案"),
                               QStringLiteral("单价/元"), QStringLiteral("累计充电次数"),
                               QStringLiteral("累计时长/分钟"), QStringLiteral("更新时间")});
    layout->addWidget(chargerTable_);
    return page;
}

QWidget *MainWindow::createUserPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("adminUserPage"));
    auto *layout = new QVBoxLayout(page);
    auto *bar = toolbar();
    auto *refresh = new QPushButton(QStringLiteral("刷新"));
    userPhoneFilterEdit_ = new QLineEdit;
    userPhoneFilterEdit_->setPlaceholderText(QStringLiteral("手机号模糊查询"));
    auto *toggle = new QPushButton(QStringLiteral("启用/停用选中用户"));
    bar->addWidget(userPhoneFilterEdit_);
    bar->addWidget(refresh);
    bar->addWidget(toggle);
    bar->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshUsers);
    connect(toggle, &QPushButton::clicked, this, &MainWindow::setUserStatus);
    connect(userPhoneFilterEdit_, &QLineEdit::returnPressed, this, &MainWindow::refreshUsers);
    userSearchDebounce_.setSingleShot(true);
    userSearchDebounce_.setInterval(300);
    connect(userPhoneFilterEdit_, &QLineEdit::textChanged, &userSearchDebounce_,
            qOverload<>(&QTimer::start));
    connect(&userSearchDebounce_, &QTimer::timeout, this, &MainWindow::refreshUsers);
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
    page->setObjectName(QStringLiteral("adminOrderPage"));
    auto *layout = new QVBoxLayout(page);
    auto *filters = new QGridLayout;
    orderNoFilterEdit_ = new QLineEdit; orderNoFilterEdit_->setPlaceholderText(QStringLiteral("订单号"));
    orderPhoneFilterEdit_ = new QLineEdit; orderPhoneFilterEdit_->setPlaceholderText(QStringLiteral("手机号"));
    orderStationFilterEdit_ = new QLineEdit; orderStationFilterEdit_->setPlaceholderText(QStringLiteral("站点名称"));
    orderStationFilterCombo_ = new QComboBox;
    orderStationFilterCombo_->addItem(QStringLiteral("全部站点"), 0);
    orderChargerFilterEdit_ = new QLineEdit; orderChargerFilterEdit_->setPlaceholderText(QStringLiteral("充电桩编号"));
    orderStatusFilterCombo_ = new QComboBox;
    orderStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString{});
    orderStatusFilterCombo_->addItem(QStringLiteral("预约中"), QStringLiteral("reserved"));
    orderStatusFilterCombo_->addItem(QStringLiteral("充电中"), QStringLiteral("charging"));
    orderStatusFilterCombo_->addItem(QStringLiteral("待结算"), QStringLiteral("pending_settlement"));
    orderStatusFilterCombo_->addItem(QStringLiteral("已结算"), QStringLiteral("settled"));
    orderStatusFilterCombo_->addItem(QStringLiteral("已取消"), QStringLiteral("cancelled"));
    orderDateFilterCheck_ = new QCheckBox(QStringLiteral("按日期"));
    orderTimeRangeCombo_ = new QComboBox;
    orderTimeRangeCombo_->addItem(QStringLiteral("全部时间"), QStringLiteral("all"));
    orderTimeRangeCombo_->addItem(QStringLiteral("今日"), QStringLiteral("today"));
    orderTimeRangeCombo_->addItem(QStringLiteral("近 7 日"), QStringLiteral("7"));
    orderTimeRangeCombo_->addItem(QStringLiteral("近 30 日"), QStringLiteral("30"));
    orderTimeRangeCombo_->addItem(QStringLiteral("自定义"), QStringLiteral("custom"));
    orderStartDateEdit_ = new QDateEdit(QDate::currentDate().addDays(-30));
    orderEndDateEdit_ = new QDateEdit(QDate::currentDate());
    orderStartDateEdit_->setCalendarPopup(true); orderEndDateEdit_->setCalendarPopup(true);
    auto *refresh = new QPushButton(QStringLiteral("刷新订单"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshOrders);
    orderDateFilterCheck_->setVisible(false);
    filters->addWidget(orderNoFilterEdit_, 0, 0); filters->addWidget(orderPhoneFilterEdit_, 0, 1);
    filters->addWidget(orderStationFilterCombo_, 0, 2); filters->addWidget(orderChargerFilterEdit_, 0, 3);
    filters->addWidget(orderStatusFilterCombo_, 1, 0); filters->addWidget(orderTimeRangeCombo_, 1, 1);
    filters->addWidget(orderStartDateEdit_, 1, 2); filters->addWidget(orderEndDateEdit_, 1, 3);
    filters->addWidget(refresh, 1, 4);
    layout->addLayout(filters);
    orderTable_ = makeTable({QStringLiteral("ID"), QStringLiteral("订单号"), QStringLiteral("用户"), QStringLiteral("手机号"),
                             QStringLiteral("站点"), QStringLiteral("充电桩"), QStringLiteral("电量/kWh"),
                             QStringLiteral("金额/元"), QStringLiteral("状态"), QStringLiteral("创建时间")});
    layout->addWidget(orderTable_);
    return page;
}

QWidget *MainWindow::createReservationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("adminReservationPage"));
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
    page->setObjectName(QStringLiteral("adminSessionPage"));
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
    page->setObjectName(QStringLiteral("adminTariffPage"));
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
    page->setObjectName(QStringLiteral("adminFaultPage"));
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


} // namespace evcs::adminclient
