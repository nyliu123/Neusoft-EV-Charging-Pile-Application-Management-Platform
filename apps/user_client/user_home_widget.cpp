#include "user_home_widget.h"

#include "user_info_widget.h"
#include "station_list_widget.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>

UserHomeWidget::UserHomeWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    successMessage_ = new QLabel(this);
    successMessage_->setStyleSheet(QStringLiteral(
        "background: #e8f5e9; color: #1b5e20; padding: 8px;"));
    successMessage_->hide();
    layout->addWidget(successMessage_);

    auto *toolbar = new QHBoxLayout;
    toolbar->addStretch();
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), this);
    toolbar->addWidget(logoutButton_);
    layout->addLayout(toolbar);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        setLogoutInProgress(true);
        emit logoutRequested();
    });
    auto *tabs = new QTabWidget(this);
    userInfoWidget_ = new UserInfoWidget(api, tabs);
    stationListWidget_ = new StationListWidget(api, tabs);
    tabs->addTab(userInfoWidget_, QStringLiteral("个人中心"));
    tabs->addTab(stationListWidget_, QStringLiteral("找充电站"));
    layout->addWidget(tabs, 1);
    connect(stationListWidget_, &StationListWidget::navigationRequested,
            this, &UserHomeWidget::navigationRequested);
    connect(stationListWidget_, &StationListWidget::pileSelected,
            this, &UserHomeWidget::pileSelected);
}

void UserHomeWidget::refresh()
{
    userInfoWidget_->refreshFromSession();
    stationListWidget_->refresh();
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
