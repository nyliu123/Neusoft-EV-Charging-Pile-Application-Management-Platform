#include "user_home_widget.h"
#include "client_ui/rhine_widgets.h"

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
#include <QHBoxLayout>
#include <QVBoxLayout>

UserHomeWidget::UserHomeWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 14, 22, 18);
    layout->setSpacing(16);

    successMessage_ = new QLabel(this);
    successMessage_->setProperty("uiClass", "successBanner");
    successMessage_->hide();
    layout->addWidget(successMessage_);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("terminalHeader"));
    auto *toolbar = new QHBoxLayout(header);
    toolbar->setContentsMargins(18, 10, 18, 12);
    toolbar->addWidget(ev::makeRhineBrand(header));
    toolbar->addStretch();
    auto *terminalLabel = new QLabel(QStringLiteral("用户工作台  /  PERSONAL TERMINAL"), header);
    terminalLabel->setProperty("uiClass", "eyebrow");
    toolbar->addWidget(terminalLabel);
    toolbar->addSpacing(16);
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), this);
    logoutButton_->setProperty("uiClass", "danger");
    toolbar->addWidget(logoutButton_);
    layout->addWidget(header);
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
