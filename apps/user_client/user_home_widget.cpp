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
    auto *shell = new QHBoxLayout(this);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("terminalSidebar"));
    sidebar->setFixedWidth(196);
    auto *rail = new QVBoxLayout(sidebar);
    rail->setContentsMargins(14, 26, 14, 14);
    rail->setSpacing(22);
    rail->addWidget(ev::makeAppleBrand(sidebar));
    auto *workspace = new QLabel(QStringLiteral("我的空间"), sidebar);
    workspace->setProperty("uiClass", "eyebrow");
    rail->addWidget(workspace);
    auto *navigation = new QListWidget(sidebar);
    navigation->setObjectName(QStringLiteral("userNavigation"));
    navigation->setAccessibleName(QStringLiteral("用户功能导航"));
    navigation->setIconSize(QSize(20, 20));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const QStringList destinations {QStringLiteral("找桩"), QStringLiteral("充电"),
        QStringLiteral("个人中心"), QStringLiteral("我的订单")};
    const ev::AppSymbol symbols[] {ev::AppSymbol::Compass, ev::AppSymbol::Bolt,
        ev::AppSymbol::Person, ev::AppSymbol::Receipt};
    for (int i = 0; i < destinations.size(); ++i)
        new QListWidgetItem(ev::appSymbolIcon(symbols[i]), destinations.at(i), navigation);
    rail->addWidget(navigation, 1);
    auto *footer = new QLabel(QStringLiteral("轻松充电，从容出发。"), sidebar);
    footer->setObjectName(QStringLiteral("terminalSidebarFooter"));
    footer->setWordWrap(true);
    rail->addWidget(footer);
    shell->addWidget(sidebar);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(26, 16, 26, 18);
    layout->setSpacing(12);
    shell->addWidget(content, 1);
    auto *header = new QFrame(content);
    header->setObjectName(QStringLiteral("terminalHeader"));
    auto *toolbar = new QHBoxLayout(header);
    toolbar->setContentsMargins(0, 0, 0, 0);
    auto *caption = new QLabel(QStringLiteral("充电服务"), header);
    caption->setProperty("uiClass", "muted");
    toolbar->addWidget(caption);
    toolbar->addStretch();
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), header);
    logoutButton_->setProperty("uiClass", "text");
    toolbar->addWidget(logoutButton_);
    layout->addWidget(header);
    successMessage_ = new QLabel(content);
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
    // Keep the established tab API and all page signals; the rail is a second view
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
