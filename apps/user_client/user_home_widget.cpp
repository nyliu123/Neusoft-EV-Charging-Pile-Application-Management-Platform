#include "user_home_widget.h"

#include "user_session_state.h"

#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

UserHomeWidget::UserHomeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    successMessage_ = new QLabel(this);
    successMessage_->setStyleSheet(QStringLiteral(
        "background: #e8f5e9; color: #1b5e20; padding: 8px;"));
    successMessage_->hide();
    layout->addWidget(successMessage_);

    avatarLabel_ = new QLabel(this);
    avatarLabel_->setFixedSize(72, 72);
    avatarLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(avatarLabel_, 0, Qt::AlignLeft);

    nicknameLabel_ = new QLabel(this);
    QFont nicknameFont = nicknameLabel_->font();
    nicknameFont.setPointSize(18);
    nicknameFont.setBold(true);
    nicknameLabel_->setFont(nicknameFont);
    layout->addWidget(nicknameLabel_);

    balanceLabel_ = new QLabel(this);
    layout->addWidget(balanceLabel_);
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), this);
    layout->addWidget(logoutButton_, 0, Qt::AlignLeft);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        setLogoutInProgress(true);
        emit logoutRequested();
    });
    layout->addWidget(new QLabel(QStringLiteral("首页业务组件将在后续分支中接入。"), this));
    layout->addStretch();
}

void UserHomeWidget::refresh()
{
    const UserSessionState &session = UserSessionState::instance();
    nicknameLabel_->setText(session.nickname().isEmpty()
        ? QStringLiteral("充电用户") : session.nickname());
    balanceLabel_->setText(QStringLiteral("钱包余额：¥%1")
        .arg(static_cast<double>(session.balanceCent()) / 100.0, 0, 'f', 2));

    const QString avatarPath = session.avatarPath();
    QPixmap avatar;
    if (!avatarPath.isEmpty() && QFileInfo::exists(avatarPath)) {
        avatar.load(avatarPath);
    }
    if (avatar.isNull()) {
        avatarLabel_->setText(QStringLiteral("默认头像"));
        avatarLabel_->setStyleSheet(QStringLiteral(
            "background: #d9d9d9; color: #555; border-radius: 36px;"));
    } else {
        avatarLabel_->setStyleSheet({});
        avatarLabel_->setPixmap(avatar.scaled(avatarLabel_->size(),
                                               Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation));
    }
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
