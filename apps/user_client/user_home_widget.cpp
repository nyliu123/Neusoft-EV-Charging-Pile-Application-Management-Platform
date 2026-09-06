#include "user_home_widget.h"

#include "user_info_widget.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
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
    userInfoWidget_ = new UserInfoWidget(api, this);
    layout->addWidget(userInfoWidget_, 1);
}

void UserHomeWidget::refresh()
{
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
