#include "user_mainwindow.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtMath>

#include <QtWebEngineWidgets/QWebEngineView>

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

} // namespace

// 本文件集中构造各个独立页面；页面样式由同名 QSS 资源控制。
QWidget *MainWindow::createLoginPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("userLoginPage"));
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
    portSpin_->setValue(8888);
    connectButton_ = new QPushButton(QStringLiteral("重新连接"));
    connectButton_->setObjectName(QStringLiteral("connectButton"));
    connectionLabel_ = new QLabel(QStringLiteral("未连接"));
    connectionForm->addRow(QStringLiteral("主机"), hostEdit_);
    connectionForm->addRow(QStringLiteral("端口"), portSpin_);
    connectionForm->addRow(connectButton_, connectionLabel_);
    layout->addWidget(connectionBox);

    auto *loginBox = new QGroupBox(QStringLiteral("用户登录"));
    auto *loginForm = new QFormLayout(loginBox);
    loginModeCombo_ = new QComboBox;
    loginModeCombo_->addItem(QStringLiteral("手机号快捷登录（首次登录自动注册）"), QStringLiteral("phone"));
    usernameEdit_ = new QLineEdit(QStringLiteral("13800138000"));
    passwordEdit_ = new QLineEdit;
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setVisible(false);
    loginButton_ = new QPushButton(QStringLiteral("登录"));
    loginButton_->setObjectName(QStringLiteral("loginButton"));
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(loginButton_);
    loginForm->addRow(QStringLiteral("登录方式"), loginModeCombo_);
    loginForm->addRow(QStringLiteral("手机号"), usernameEdit_);
    loginForm->addRow(buttons);
    layout->addWidget(loginBox);
    layout->addStretch();

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectServer);
    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(loginModeCombo_, &QComboBox::currentIndexChanged,
            this, &MainWindow::updateLoginMode);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &MainWindow::login);
    updateLoginMode();
    return page;
}

QWidget *MainWindow::createStationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("userStationPage"));
    auto *layout = new QVBoxLayout(page);
    stationViewStack_ = new QStackedWidget;
    layout->addWidget(stationViewStack_);

    stationListView_ = new QWidget;
    auto *listLayout = new QVBoxLayout(stationListView_);
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
    listLayout->addLayout(search);

    auto *locationBar = new QHBoxLayout;
    locationPresetCombo_ = new QComboBox;
    locationPresetCombo_->addItems({QStringLiteral("选择预设区域"), QStringLiteral("北京市海淀区中关村"),
                                    QStringLiteral("北京市朝阳区望京"), QStringLiteral("北京市东城区"),
                                    QStringLiteral("北京市大兴区亦庄")});
    locationEdit_ = new QLineEdit;
    locationEdit_->setPlaceholderText(QStringLiteral("当前位置/地址，例如：北京市海淀区中关村"));
    auto *geocode = new QPushButton(QStringLiteral("解析当前位置"));
    auto *drive = new QPushButton(QStringLiteral("驾车导航"));
    auto *walk = new QPushButton(QStringLiteral("步行导航"));
    locationStatusLabel_ = new QLabel(QStringLiteral("尚未设置当前位置"));
    locationBar->addWidget(locationPresetCombo_);
    locationBar->addWidget(locationEdit_, 2);
    locationBar->addWidget(geocode);
    locationBar->addWidget(drive);
    locationBar->addWidget(walk);
    locationBar->addWidget(locationStatusLabel_, 2);
    listLayout->addLayout(locationBar);

    stationCardList_ = new QListWidget;
    stationCardList_->setObjectName(QStringLiteral("stationCardList"));
    stationCardList_->setSpacing(8);
    stationCardList_->setSelectionMode(QAbstractItemView::NoSelection);
    listLayout->addWidget(stationCardList_);

    stationTable_ = new QTableWidget;
    stationTable_->setObjectName(QStringLiteral("stationTable"));
    configureTable(stationTable_, {QStringLiteral("编号"), QStringLiteral("名称"),
                                   QStringLiteral("区域"), QStringLiteral("地址"),
                                   QStringLiteral("空闲/全部"), QStringLiteral("在线率"),
                                   QStringLiteral("距离"), QStringLiteral("最低价格")});
    stationTable_->setVisible(false);
    listLayout->addWidget(stationTable_);
    stationViewStack_->addWidget(stationListView_);

    stationDetailView_ = new QWidget;
    auto *detailLayout = new QVBoxLayout(stationDetailView_);
    auto *detailTop = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("← 返回站点列表"));
    stationDetailTitle_ = new QLabel(QStringLiteral("<h2>充电站详情</h2>"));
    detailTop->addWidget(back);
    detailTop->addWidget(stationDetailTitle_);
    detailTop->addStretch();
    detailLayout->addLayout(detailTop);
    stationDetailAddress_ = new QLabel(QStringLiteral("地址：--"));
    stationDetailSummary_ = new QLabel(QStringLiteral("价格、在线率：--"));
    detailLayout->addWidget(stationDetailAddress_);
    detailLayout->addWidget(stationDetailSummary_);

    chargerTable_ = new QTableWidget;
    configureTable(chargerTable_, {QStringLiteral("编号"), QStringLiteral("桩编号"),
                                   QStringLiteral("类型/接口"), QStringLiteral("功率"), QStringLiteral("状态"),
                                   QStringLiteral("价格")});
    detailLayout->addWidget(chargerTable_, 2);
    auto *chargerButtons = new QHBoxLayout;
    auto *reserve = new QPushButton(QStringLiteral("预约所选充电桩"));
    chargerButtons->addWidget(reserve);
    chargerButtons->addStretch();
    detailLayout->addLayout(chargerButtons);
    stationViewStack_->addWidget(stationDetailView_);

    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshStations);
    connect(locationPresetCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (locationPresetCombo_->currentIndex() > 0) locationEdit_->setText(text);
    });
    connect(geocode, &QPushButton::clicked, this, &MainWindow::geocodeLocation);
    connect(drive, &QPushButton::clicked, this,
            [this] { navigateSelectedStation(QStringLiteral("drive")); });
    connect(walk, &QPushButton::clicked, this,
            [this] { navigateSelectedStation(QStringLiteral("walk")); });
    connect(stationTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { loadSelectedStation(); });
    connect(back, &QPushButton::clicked, this, [this] {
        stationViewStack_->setCurrentWidget(stationListView_);
    });
    connect(reserve, &QPushButton::clicked, this, &MainWindow::reserveSelectedCharger);
    return page;
}

QWidget *MainWindow::createReservationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("userReservationPage"));
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
    page->setObjectName(QStringLiteral("userChargingPage"));
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
    page->setObjectName(QStringLiteral("userOrderPage"));
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
    page->setObjectName(QStringLiteral("userProfilePage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(100, 80, 100, 80);
    profileAvatarLabel_ = new QLabel(QStringLiteral("暂无头像"));
    profileAvatarLabel_->setFixedSize(128, 128);
    profileAvatarLabel_->setAlignment(Qt::AlignCenter);
    profileAvatarLabel_->setStyleSheet(QStringLiteral(
        "border: 1px solid #aab7c4; border-radius: 64px; background: #eef3f7;"));
    layout->addWidget(profileAvatarLabel_, 0, Qt::AlignHCenter);
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
    auto *avatar = new QPushButton(QStringLiteral("更换头像"));
    auto *rename = new QPushButton(QStringLiteral("修改昵称"));
    auto *recharge = new QPushButton(QStringLiteral("钱包充值"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"));
    buttons->addWidget(refresh);
    buttons->addWidget(avatar);
    buttons->addWidget(rename);
    buttons->addWidget(recharge);
    buttons->addStretch();
    buttons->addWidget(logoutButton);
    layout->addLayout(buttons);
    layout->addStretch();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshProfile);
    connect(avatar, &QPushButton::clicked, this, &MainWindow::chooseAvatar);
    connect(rename, &QPushButton::clicked, this, &MainWindow::editDisplayName);
    connect(recharge, &QPushButton::clicked, this, &MainWindow::rechargeWallet);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    return page;
}


} // namespace evcs::userclient
