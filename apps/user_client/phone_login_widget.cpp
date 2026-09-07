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
    setFixedSize(380, 280);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("用户服务中心"), this);
    title->setProperty("uiClass", "dialogTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *subTitle = new QLabel(QStringLiteral("电动汽车充电桩管理平台"), this);
    subTitle->setProperty("uiClass", "muted");
    subTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subTitle);

    layout->addSpacing(8);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(10);

    phoneInput_ = new QLineEdit(this);
    phoneInput_->setPlaceholderText(QStringLiteral("请输入11位大陆手机号"));
    phoneInput_->setClearButtonEnabled(true);
    phoneInput_->setInputMethodHints(Qt::ImhDigitsOnly);
    phoneInput_->setAccessibleName(QStringLiteral("手机号"));
    formLayout->addRow(QStringLiteral("手机号："), phoneInput_);
    layout->addLayout(formLayout);

    phoneMessage_ = new QLabel(this);
    phoneMessage_->setProperty("uiClass", "feedback");
    phoneMessage_->setWordWrap(true);
    phoneMessage_->setAccessibleName(QStringLiteral("手机号校验结果"));
    phoneMessage_->setVisible(false);
    layout->addWidget(phoneMessage_);

    layout->addStretch();

    loginButton_ = new QPushButton(QStringLiteral("登 录"), this);
    loginButton_->setProperty("uiClass", "primary");
    loginButton_->setDefault(true);
    loginButton_->setMinimumHeight(36);
    layout->addWidget(loginButton_);

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
