#include "phone_login_widget.h"

#include "common/phone_validator.h"

#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringView>
#include <QVBoxLayout>

PhoneLoginWidget::PhoneLoginWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel(
        QStringLiteral("请输入手机号登录演示系统。本项目不发送短信验证码，也不验证手机号归属。"),
        this));
    layout->addWidget(new QLabel(QStringLiteral("手机号"), this));

    phoneInput_ = new QLineEdit(this);
    phoneInput_->setPlaceholderText(QStringLiteral("请输入11位大陆手机号"));
    phoneInput_->setClearButtonEnabled(true);
    phoneInput_->setInputMethodHints(Qt::ImhDigitsOnly);
    phoneInput_->setAccessibleName(QStringLiteral("手机号"));
    layout->addWidget(phoneInput_);

    phoneMessage_ = new QLabel(this);
    phoneMessage_->setWordWrap(true);
    phoneMessage_->setAccessibleName(QStringLiteral("手机号校验结果"));
    layout->addWidget(phoneMessage_);

    loginButton_ = new QPushButton(QStringLiteral("登录"), this);
    layout->addWidget(loginButton_, 0, Qt::AlignLeft);

    connect(loginButton_, &QPushButton::clicked, this, [this] { validatePhone(); });
    connect(phoneInput_, &QLineEdit::returnPressed, this, [this] { validatePhone(); });
    connect(phoneInput_, &QLineEdit::textEdited, this, [this] {
        phoneMessage_->clear();
        phoneMessage_->setStyleSheet({});
    });
}

void PhoneLoginWidget::setLoginInProgress(bool inProgress)
{
    loginButton_->setEnabled(!inProgress);
    loginButton_->setText(inProgress ? QStringLiteral("正在登录...")
                                     : QStringLiteral("登录"));
}

void PhoneLoginWidget::showLoginError(const QString &message)
{
    phoneMessage_->setText(message);
    phoneMessage_->setStyleSheet(QStringLiteral("color: #b3261e;"));
    setLoginInProgress(false);
    QMessageBox::warning(this, QStringLiteral("提示"), message);
    phoneInput_->setFocus();
}

void PhoneLoginWidget::resetForLogin()
{
    phoneMessage_->clear();
    phoneMessage_->setStyleSheet({});
    setLoginInProgress(false);
    phoneInput_->setFocus();
}

void PhoneLoginWidget::validatePhone()
{
    const ev::PhoneValidationResult result =
        ev::PhoneValidator::validate(QStringView(phoneInput_->text()));
    if (!result.isValid()) {
        showValidationError(result.error);
        return;
    }

    phoneMessage_->setText(QStringLiteral("手机号格式正确，正在登录..."));
    phoneMessage_->setStyleSheet(QStringLiteral("color: #1b5e20;"));
    setLoginInProgress(true);
    emit phoneAccepted(phoneInput_->text());
}

void PhoneLoginWidget::showValidationError(ev::PhoneValidationError error)
{
    const QString message = ev::PhoneValidator::errorMessage(error);
    phoneMessage_->setText(message);
    phoneMessage_->setStyleSheet(QStringLiteral("color: #b3261e;"));

    QMessageBox::warning(this, QStringLiteral("提示"), message);
    if (error == ev::PhoneValidationError::InvalidFormat) {
        phoneInput_->clear();
    }
    phoneInput_->setFocus();
}
