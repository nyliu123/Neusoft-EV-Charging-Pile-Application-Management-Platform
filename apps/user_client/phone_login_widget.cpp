#include "phone_login_widget.h"

#include "common/phone_validator.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QStringView>
#include <QVBoxLayout>

namespace {

void setTone(QWidget *widget, const char *tone)
{
    widget->setProperty("tone", tone);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

PhoneLoginWidget::PhoneLoginWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("userLoginCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(360, 360);
    setMaximumWidth(440);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(16);

    auto *eyebrow = new QLabel(QStringLiteral("01 / USER ACCESS"), this);
    eyebrow->setProperty("uiClass", "eyebrow");
    layout->addWidget(eyebrow);
    auto *title = new QLabel(QStringLiteral("欢迎接入"), this);
    title->setProperty("uiClass", "dialogTitle");
    title->setAlignment(Qt::AlignLeft);
    layout->addWidget(title);

    auto *subTitle = new QLabel(QStringLiteral("充电服务终端 · 登录后开始你的旅程"), this);
    subTitle->setProperty("uiClass", "muted");
    subTitle->setAlignment(Qt::AlignLeft);
    layout->addWidget(subTitle);

    layout->addSpacing(8);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(10);

    auto *phoneLabel = new QLabel(QStringLiteral("手机号 / PHONE NUMBER"), this);
    phoneLabel->setProperty("uiClass", "formLabel");
    formLayout->addRow(phoneLabel);
    phoneInput_ = new QLineEdit(this);
    phoneInput_->setObjectName(QStringLiteral("userPhoneInput"));
    phoneInput_->setPlaceholderText(QStringLiteral("请输入11位大陆手机号"));
    phoneInput_->setClearButtonEnabled(true);
    phoneInput_->setInputMethodHints(Qt::ImhDigitsOnly);
    phoneInput_->setAccessibleName(QStringLiteral("手机号"));
    formLayout->addRow(phoneInput_);
    layout->addLayout(formLayout);

    phoneMessage_ = new QLabel(this);
    phoneMessage_->setProperty("uiClass", "feedback");
    phoneMessage_->setWordWrap(true);
    phoneMessage_->setAccessibleName(QStringLiteral("手机号校验结果"));
    phoneMessage_->setVisible(false);
    layout->addWidget(phoneMessage_);

    layout->addStretch();

    loginButton_ = new QPushButton(QStringLiteral("登 录"), this);
    loginButton_->setObjectName(QStringLiteral("userLoginButton"));
    loginButton_->setProperty("uiClass", "primary");
    loginButton_->setDefault(true);
    loginButton_->setMinimumHeight(36);
    layout->addWidget(loginButton_);
    auto *hint = new QLabel(QStringLiteral("未注册的手机号将自动创建账号。"), this);
    hint->setProperty("uiClass", "muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(loginButton_, &QPushButton::clicked, this, [this] { validatePhone(); });
    connect(phoneInput_, &QLineEdit::returnPressed, this, [this] { validatePhone(); });
    connect(phoneInput_, &QLineEdit::textEdited, this, [this] {
        phoneMessage_->clear();
        phoneMessage_->setVisible(false);
        setTone(phoneMessage_, "");
    });
}

void PhoneLoginWidget::setLoginInProgress(bool inProgress)
{
    loginButton_->setEnabled(!inProgress);
    loginButton_->setText(inProgress ? QStringLiteral("正在登录...")
                                     : QStringLiteral("登 录"));
}

void PhoneLoginWidget::showLoginError(const QString &message)
{
    phoneMessage_->setText(message);
    phoneMessage_->setVisible(true);
    setTone(phoneMessage_, "error");
    setLoginInProgress(false);
    QMessageBox::warning(this, QStringLiteral("提示"), message);
    phoneInput_->setFocus();
}

void PhoneLoginWidget::resetForLogin()
{
    phoneMessage_->clear();
    phoneMessage_->setVisible(false);
    setTone(phoneMessage_, "");
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
    phoneMessage_->setVisible(true);
    setTone(phoneMessage_, "success");
    setLoginInProgress(true);
    emit phoneAccepted(phoneInput_->text());
}

void PhoneLoginWidget::showValidationError(ev::PhoneValidationError error)
{
    const QString message = ev::PhoneValidator::errorMessage(error);
    phoneMessage_->setText(message);
    phoneMessage_->setVisible(true);
    setTone(phoneMessage_, "error");

    QMessageBox::warning(this, QStringLiteral("提示"), message);
    if (error == ev::PhoneValidationError::InvalidFormat) {
        phoneInput_->clear();
    }
    phoneInput_->setFocus();
}
