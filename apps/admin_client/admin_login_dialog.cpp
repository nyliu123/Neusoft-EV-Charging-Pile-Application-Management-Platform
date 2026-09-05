#include "admin_login_dialog.h"

#include "admin_session.h"
#include "common/protocol.h"
#include "network/platform_client.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ev {

AdminLoginDialog::AdminLoginDialog(PlatformClient *client, QWidget *parent)
    : QDialog(parent), client_(client)
{
    setWindowTitle(QStringLiteral("管理员登录"));
    setFixedSize(380, 280);
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 28, 32, 24);
    mainLayout->setSpacing(16);

    // Title.
    titleLabel_ = new QLabel(QStringLiteral("运营管理中心"), this);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel_);

    auto *subTitle = new QLabel(QStringLiteral("电动汽车充电桩管理平台"), this);
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet(QStringLiteral("color: #888;"));
    mainLayout->addWidget(subTitle);

    mainLayout->addSpacing(8);

    // Form fields.
    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText(QStringLiteral("请输入用户名"));
    usernameEdit_->setClearButtonEnabled(true);
    formLayout->addRow(QStringLiteral("用户名："), usernameEdit_);

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
    formLayout->addRow(QStringLiteral("密　码："), passwordEdit_);

    mainLayout->addLayout(formLayout);

    // Error label.
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet(QStringLiteral("color: #e53935; font-size: 12px;"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    mainLayout->addWidget(errorLabel_);

    mainLayout->addStretch();

    // Login button.
    loginButton_ = new QPushButton(QStringLiteral("登 录"), this);
    loginButton_->setDefault(true);
    loginButton_->setMinimumHeight(36);
    loginButton_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background-color: #1976d2;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #1565c0; }"
        "QPushButton:pressed { background-color: #0d47a1; }"
        "QPushButton:disabled { background-color: #90caf9; }"
    ));
    mainLayout->addWidget(loginButton_);

    // Connections.
    connect(loginButton_, &QPushButton::clicked, this, &AdminLoginDialog::onLoginClicked);
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
    const QString username = usernameEdit_->text().trimmed();
    const QString password = passwordEdit_->text();

    if (username.isEmpty()) {
        showError(QStringLiteral("请输入用户名"));
        usernameEdit_->setFocus();
        return;
    }
    if (password.isEmpty()) {
        showError(QStringLiteral("请输入密码"));
        passwordEdit_->setFocus();
        return;
    }

    if (client_->state() != PlatformClient::State::Ready) {
        showError(QStringLiteral("服务端未连接，请检查网络或稍后重试"));
        return;
    }

    setLoading(true);
    errorLabel_->setVisible(false);

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
    loginButton_->setText(loading ? QStringLiteral("登录中...")
                                  : QStringLiteral("登 录"));
}

void AdminLoginDialog::showError(const QString &message)
{
    errorLabel_->setText(message);
    errorLabel_->setVisible(true);
}

} // namespace ev
