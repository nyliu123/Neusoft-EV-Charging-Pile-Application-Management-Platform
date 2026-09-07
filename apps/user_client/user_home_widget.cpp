#include "user_home_widget.h"

#include "user_info_widget.h"
#include "station_search_widget.h"

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

    successMessage_ = new QLabel(this);
    successMessage_->setProperty("uiClass", "successBanner");
    successMessage_->hide();
    layout->addWidget(successMessage_);

    auto *toolbar = new QHBoxLayout;
    toolbar->addStretch();
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), this);
    logoutButton_->setProperty("uiClass", "danger");
    toolbar->addWidget(logoutButton_);
    layout->addLayout(toolbar);
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
    stationSearchWidget_ = new StationSearchWidget(api, tabs);
    userInfoWidget_ = new UserInfoWidget(api, tabs);
    tabs->addTab(stationSearchWidget_, QStringLiteral("找桩"));
    tabs->addTab(userInfoWidget_, QStringLiteral("个人中心"));
    layout->addWidget(tabs, 1);
}

void UserHomeWidget::refresh()
{
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
