#include "user_home_widget.h"
#include "client_ui/apple_widgets.h"

#include "charge_flow_widget.h"
#include "user_info_widget.h"
#include "station_search_widget.h"
#include "order_list_widget.h"
#include "user_api_client.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>

UserHomeWidget::UserHomeWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 14, 24, 14);
    layout->setSpacing(16);
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("userTopBar"));
    auto *toolbar = new QHBoxLayout(header);
    toolbar->setContentsMargins(14, 5, 14, 5);
    toolbar->setSpacing(12);
    auto *brand = ev::makeAppleBrand(header);
    brand->setFixedWidth(142);
    toolbar->addWidget(brand);
    toolbar->addStretch();
    auto *navigation = new QListWidget(header);
    navigation->setObjectName(QStringLiteral("userNavigation"));
    navigation->setAccessibleName(QStringLiteral("用户功能导航"));
    navigation->setIconSize(QSize(18, 18));
    navigation->setFlow(QListView::LeftToRight);
    navigation->setWrapping(false);
    navigation->setMovement(QListView::Static);
    navigation->setResizeMode(QListView::Adjust);
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setFixedSize(518, 46);
    const QStringList destinations {QStringLiteral("找桩"), QStringLiteral("充电"),
        QStringLiteral("个人中心"), QStringLiteral("我的订单")};
    const ev::AppSymbol symbols[] {ev::AppSymbol::Compass, ev::AppSymbol::Bolt,
        ev::AppSymbol::Person, ev::AppSymbol::Receipt};
    for (int i = 0; i < destinations.size(); ++i) {
        auto *item = new QListWidgetItem(ev::appSymbolIcon(symbols[i]), destinations.at(i), navigation);
        item->setSizeHint(QSize(126, 36));
    }
    toolbar->addWidget(navigation);
    toolbar->addStretch();
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), header);
    logoutButton_->setProperty("uiClass", "text");
    toolbar->addWidget(logoutButton_);
    layout->addWidget(header);
    successMessage_ = new QLabel(this);
    successMessage_->setProperty("uiClass", "successBanner");
    successMessage_->hide();
    layout->addWidget(successMessage_);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(this, QStringLiteral("退出登录"),
            QStringLiteral("确定退出当前用户账号？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        setLogoutInProgress(true);
        emit logoutRequested();
    });
    auto *tabs = new QTabWidget(this);
    tabs_ = tabs;
    stationSearchWidget_ = new StationSearchWidget(api, tabs);
    chargeFlowWidget_ = new ChargeFlowWidget(api, tabs);
    connect(api, &ev::UserApiClient::sessionExpired, chargeFlowWidget_, &ChargeFlowWidget::reset);
    userInfoWidget_ = new UserInfoWidget(api, tabs);
    tabs->addTab(stationSearchWidget_, QStringLiteral("找桩"));
    tabs->addTab(chargeFlowWidget_, QStringLiteral("充电"));
    tabs->addTab(userInfoWidget_, QStringLiteral("个人中心"));
    orderListWidget_ = new OrderListWidget(api, tabs);
    tabs->addTab(orderListWidget_, QStringLiteral("我的订单"));
    connect(tabs, &QTabWidget::currentChanged, this, [this](int) {
        if (tabs_->currentWidget() == orderListWidget_) {
            orderListWidget_->refresh();
        } else {
            orderListWidget_->reset();
        }
    });
    connect(orderListWidget_, &OrderListWidget::backRequested, this, [this] {
        tabs_->setCurrentWidget(stationSearchWidget_);
    });
    // Keep the established tab API and all page signals; the navigation is a second view
    // of the same selection state, including programmatic charge/profile jumps.
    tabs->tabBar()->hide();
    connect(navigation, &QListWidget::currentRowChanged, tabs, &QTabWidget::setCurrentIndex);
    connect(tabs, &QTabWidget::currentChanged, navigation, QOverload<int>::of(&QListWidget::setCurrentRow));
    navigation->setCurrentRow(tabs->currentIndex());
    layout->addWidget(tabs, 1);

    connect(stationSearchWidget_, &StationSearchWidget::pileChosen, this, [this, tabs](qint64 pileId) {
        chargeFlowWidget_->enterWithPile(pileId);
        tabs->setCurrentWidget(chargeFlowWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::pileSelectionRequested, this, [this, tabs] {
        tabs->setCurrentWidget(stationSearchWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::rechargeRequested, this, [this, tabs] {
        tabs->setCurrentWidget(userInfoWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::homeRequested, this, [this, tabs] {
        tabs->setCurrentWidget(stationSearchWidget_);
    });
    // UML-025: entering the charge tab always re-checks pending orders first.
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        if (tabs->widget(index) == chargeFlowWidget_) {
            chargeFlowWidget_->enterFromHome();
        }
    });
}

void UserHomeWidget::refresh()
{
    orderListWidget_->reset();
    chargeFlowWidget_->reset();
    tabs_->setCurrentWidget(stationSearchWidget_);
    stationSearchWidget_->refresh();
    userInfoWidget_->refreshFromSession();
}

void UserHomeWidget::showWelcome(bool isNewUser)
{
    successMessage_->setText(isNewUser
        ? QStringLiteral("注册成功，欢迎加入！")
        : QStringLiteral("登录成功"));
    successMessage_->show();
    QTimer::singleShot(3000, successMessage_, &QLabel::hide);
}

void UserHomeWidget::setLogoutInProgress(bool inProgress)
{
    logoutButton_->setEnabled(!inProgress);
    logoutButton_->setText(inProgress ? QStringLiteral("正在退出...")
                                      : QStringLiteral("退出登录"));
}
