#include "admin_login_dialog.h"
#include "admin_session.h"
#include "client_ui/apple_widgets.h"
#include "client_ui/eye_line_edit.h"
#include "common/protocol.h"
#include "network/platform_client.h"
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace ev {
namespace {
QString rememberedUsername;
QString rememberedPassword;
}

AdminLoginDialog::AdminLoginDialog(PlatformClient *client, QWidget *parent)
    : QDialog(parent), client_(client)
{
    setObjectName(QStringLiteral("adminLoginDialog"));
    setWindowTitle(QStringLiteral("轻充 · 管理员登录"));
    resize(1000, 640);
    setMinimumSize(880, 600);
    setModal(true);
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(0);
    root->addWidget(new AppleIdentityPanel(false, this), 1);
    auto *formArea = new QWidget(this);
    auto *areaLayout = new QVBoxLayout(formArea);
    areaLayout->setContentsMargins(30, 28, 30, 28);
    auto *card = new QWidget(formArea);
    card->setObjectName(QStringLiteral("adminLoginCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setMinimumWidth(360);
    card->setMaximumWidth(440);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 32, 32, 30);
    cardLayout->setSpacing(14);
    auto *eyebrow = new QLabel(QStringLiteral("轻充工作空间"), card);
    eyebrow->setProperty("uiClass", "eyebrow");
    cardLayout->addWidget(eyebrow);
    titleLabel_ = new QLabel(QStringLiteral("登录管理端"), card);
    titleLabel_->setProperty("uiClass", "dialogTitle");
    cardLayout->addWidget(titleLabel_);
    subTitleLabel_ = new QLabel(QStringLiteral("管理站点、设备和每一笔充电订单。"), card);
    subTitleLabel_->setProperty("uiClass", "muted");
    cardLayout->addWidget(subTitleLabel_);
    cardLayout->addSpacing(12);
    auto *usernameLabel = new QLabel(QStringLiteral("管理员账号"), card);
    usernameLabel->setProperty("uiClass", "formLabel");
    cardLayout->addWidget(usernameLabel);
    usernameEdit_ = new QLineEdit(card);
    usernameEdit_->setObjectName(QStringLiteral("adminUsernameInput"));
    usernameEdit_->setAccessibleName(QStringLiteral("管理员账号"));
    usernameEdit_->setPlaceholderText(QStringLiteral("请输入用户名"));
    usernameEdit_->setClearButtonEnabled(true);
    usernameEdit_->setMaxLength(50);
    usernameEdit_->setText(rememberedUsername);
    cardLayout->addWidget(usernameEdit_);
    auto *passwordLabel = new QLabel(QStringLiteral("密码"), card);
    passwordLabel->setProperty("uiClass", "formLabel");
    cardLayout->addWidget(passwordLabel);
    auto *passwordInput = new EyeLineEdit(card);
    passwordEdit_ = passwordInput;
    passwordEdit_->setObjectName(QStringLiteral("adminPasswordInput"));
    passwordEdit_->setAccessibleName(QStringLiteral("管理员密码"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit_->setMaxLength(255);
    passwordEdit_->setText(rememberedPassword);
    passwordToggle_ = passwordInput->eyeButton();
    passwordInput->setEyeVisible(false);
    passwordToggle_->setToolTip(QStringLiteral("显示密码"));
    cardLayout->addWidget(passwordEdit_);
    errorLabel_ = new QLabel(card);
    errorLabel_->setProperty("uiClass", "errorText");
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    cardLayout->addWidget(errorLabel_);
    cardLayout->addSpacing(8);
    loginButton_ = new QPushButton(QStringLiteral("登录"), card);
    loginButton_->setObjectName(QStringLiteral("adminLoginButton"));
    loginButton_->setProperty("uiClass", "primary");
    loginButton_->setCursor(Qt::PointingHandCursor);
    loginButton_->setDefault(true);
    loginButton_->setMinimumHeight(28);
    cardLayout->addWidget(loginButton_);
    hintLabel_ = new QLabel(QStringLiteral("默认账号：admin  /  默认密码：admin123"), card);
    hintLabel_->setProperty("uiClass", "muted");
    hintLabel_->setWordWrap(true);
    cardLayout->addWidget(hintLabel_);
    areaLayout->addStretch();
    areaLayout->addWidget(card, 0, Qt::AlignHCenter);
    areaLayout->addStretch();
    root->addWidget(formArea, 1);

    // ── Connections ───────────────────────────────────────────────────────
    connect(loginButton_, &QPushButton::clicked,
            this, &AdminLoginDialog::onLoginClicked);
    connect(passwordToggle_, &QToolButton::clicked, this, [this]() {
        passwordVisible_ = !passwordVisible_;
        passwordEdit_->setEchoMode(passwordVisible_
            ? QLineEdit::Normal : QLineEdit::Password);
        static_cast<EyeLineEdit *>(passwordEdit_)->setEyeVisible(passwordVisible_);
        passwordToggle_->setToolTip(passwordVisible_ ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });
    connect(passwordEdit_, &QLineEdit::returnPressed,
            loginButton_, &QPushButton::click);
    connect(usernameEdit_, &QLineEdit::returnPressed,
            passwordEdit_, QOverload<>::of(&QLineEdit::setFocus));

    // Connect to client for login response.
    connect(client_, &PlatformClient::frameReceived,
            this, [this](quint32 messageType, const QJsonObject &payload) {
        if (messageType != static_cast<quint32>(MessageType::LoginResponse)) {
            return;
        }
        const QString requestId = payload.value(QStringLiteral("request_id")).toString();
        if (requestId != pendingRequestId_) {
            return;
        }
        const bool success = payload.value(QStringLiteral("success")).toBool();
        const QString message = payload.value(QStringLiteral("message")).toString();

        AdminSession session;
        if (success) {
            const QJsonObject data = payload.value(QStringLiteral("data")).toObject();
            session.adminId = data.value(QStringLiteral("admin_id")).toInt();
            session.username = data.value(QStringLiteral("username")).toString();
            session.sessionId = data.value(QStringLiteral("session_id")).toString();
            session.isLoggedIn = true;
        }
        onLoginResponse(success, message, session);
    });

    // Set default focus.
    usernameEdit_->setFocus();
}

void AdminLoginDialog::onLoginClicked()
{
    clearError();

    const QString username = usernameEdit_->text().trimmed();
    const QString password = passwordEdit_->text();

    if (!validateInput(username, password)) {
        return;
    }

    if (client_->state() != PlatformClient::State::Ready) {
        showError(QStringLiteral("服务端未连接，请检查网络或稍后重试"));
        return;
    }

    setLoading(true);

    const QJsonObject data {
        {QStringLiteral("username"), username},
        {QStringLiteral("password"), password},
        {QStringLiteral("role"), QStringLiteral("admin")}
    };
    pendingRequestId_ = client_->sendFrame(
        static_cast<quint32>(MessageType::LoginRequest), data);

    if (pendingRequestId_.isEmpty()) {
        setLoading(false);
        showError(QStringLiteral("发送登录请求失败，请检查连接"));
    }
}

void AdminLoginDialog::onLoginResponse(bool success, const QString &message,
                                       const AdminSession &session)
{
    setLoading(false);
    if (success) {
        rememberedUsername = usernameEdit_->text().trimmed();
        rememberedPassword = passwordEdit_->text();
        session_ = session;
        accept();
    } else {
        showError(message.isEmpty() ? QStringLiteral("登录失败") : message);
        passwordEdit_->selectAll();
        passwordEdit_->setFocus();
    }
}

void AdminLoginDialog::setLoading(bool loading)
{
    loginButton_->setEnabled(!loading);
    usernameEdit_->setEnabled(!loading);
    passwordEdit_->setEnabled(!loading);
    passwordToggle_->setEnabled(!loading);
    loginButton_->setText(loading ? QStringLiteral("登录中...")
                                  : QStringLiteral("登录"));
}

void AdminLoginDialog::showError(const QString &message)
{
    errorLabel_->setText(QStringLiteral("⚠  %1").arg(message));
    errorLabel_->setVisible(true);
}

void AdminLoginDialog::clearError()
{
    errorLabel_->clear();
    errorLabel_->setVisible(false);
}

bool AdminLoginDialog::validateInput(const QString &username, const QString &password)
{
    if (username.isEmpty()) {
        showError(QStringLiteral("请输入用户名"));
        usernameEdit_->setFocus();
        return false;
    }
    if (password.isEmpty()) {
        showError(QStringLiteral("请输入密码"));
        passwordEdit_->setFocus();
        return false;
    }
    if (username.length() > 50) {
        showError(QStringLiteral("用户名长度不能超过 50 个字符"));
        return false;
    }
    if (password.length() > 255) {
        showError(QStringLiteral("密码长度不能超过 255 个字符"));
        return false;
    }
    return true;
}

} // namespace ev
