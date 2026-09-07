#include "admin_login_dialog.h"

#include "admin_session.h"
#include "common/protocol.h"
#include "network/platform_client.h"

#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace ev {

// ── Style helpers ───────────────────────────────────────────────────────────

static const char *kInputStyle =
    "QLineEdit {"
    "  background: #f5f7fa;"
    "  border: 1.5px solid #e4e7ed;"
    "  border-radius: 6px;"
    "  padding: 10px 12px;"
    "  font-size: 14px;"
    "  color: #303133;"
    "  selection-background-color: #409eff;"
    "}"
    "QLineEdit:focus {"
    "  border-color: #409eff;"
    "  background: white;"
    "}"
    "QLineEdit::placeholder { color: #a8abb2; }";

static const char *kLoginButtonStyle =
    "QPushButton {"
    "  background-color: #409eff;"
    "  color: white;"
    "  border: none;"
    "  border-radius: 6px;"
    "  font-size: 15px;"
    "  font-weight: 600;"
    "  padding: 11px 0;"
    "}"
    "QPushButton:hover { background-color: #66b1ff; }"
    "QPushButton:pressed { background-color: #3a8ee6; }"
    "QPushButton:disabled { background-color: #a0cfff; }";

static const char *kToggleStyle =
    "QPushButton {"
    "  background: transparent;"
    "  border: none;"
    "  color: #909399;"
    "  font-size: 13px;"
    "  padding: 4px 8px;"
    "}"
    "QPushButton:hover { color: #409eff; }";

// ── AdminLoginDialog ──────────────────────────────────────────────────────────

AdminLoginDialog::AdminLoginDialog(PlatformClient *client, QWidget *parent)
    : QDialog(parent), client_(client)
{
    setObjectName(QStringLiteral("adminLoginDialog"));
    setWindowTitle(QStringLiteral("管理员登录"));
    setFixedSize(440, 560);
    setModal(true);
    // Transparent background so the gradient paintEvent shows through.
    setAttribute(Qt::WA_StyledBackground, false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setAlignment(Qt::AlignCenter);

    // ── White card ──────────────────────────────────────────────────────
    auto *card = new QWidget(this);
    card->setFixedSize(360, 440);
    card->setStyleSheet(QStringLiteral(
        "QWidget { background: white; border-radius: 12px; }"));

    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    card->setGraphicsEffect(shadow);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(36, 36, 36, 28);
    cardLayout->setSpacing(0);
    cardLayout->setAlignment(Qt::AlignTop);

    // ── Logo badge ──────────────────────────────────────────────────────
    auto *logoLabel = new QLabel(card);
    logoLabel->setText(QStringLiteral("EV"));
    logoLabel->setFixedSize(56, 56);
    logoLabel->setAlignment(Qt::AlignCenter);
    QFont logoFont = logoLabel->font();
    logoFont.setPointSize(18);
    logoFont.setBold(true);
    logoLabel->setFont(logoFont);
    logoLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #409eff, stop:1 #1976d2);"
        "  color: white;"
        "  border-radius: 28px;"
        "}"));
    auto *logoLayout = new QHBoxLayout();
    logoLayout->setAlignment(Qt::AlignCenter);
    logoLayout->addWidget(logoLabel);
    cardLayout->addLayout(logoLayout);
    cardLayout->addSpacing(18);

    // ── Title ────────────────────────────────────────────────────────────
    titleLabel_ = new QLabel(card);
    titleLabel_->setText(QStringLiteral("充电桩运营管理平台"));
    titleLabel_->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setStyleSheet(QStringLiteral("color: #303133;"));
    cardLayout->addWidget(titleLabel_);

    subTitleLabel_ = new QLabel(QStringLiteral("Admin Console"), card);
    subTitleLabel_->setAlignment(Qt::AlignCenter);
    QFont subFont = subTitleLabel_->font();
    subFont.setPointSize(10);
    subTitleLabel_->setFont(subFont);
    subTitleLabel_->setStyleSheet(QStringLiteral("color: #909399; letter-spacing: 2px;"));
    cardLayout->addWidget(subTitleLabel_);

    cardLayout->addSpacing(24);

    // ── Form fields ─────────────────────────────────────────────────────
    usernameEdit_ = new QLineEdit(card);
    usernameEdit_->setPlaceholderText(QStringLiteral("请输入用户名"));
    usernameEdit_->setClearButtonEnabled(true);
    usernameEdit_->setMaxLength(50);
    usernameEdit_->setStyleSheet(QString::fromLatin1(kInputStyle));
    usernameEdit_->setMinimumHeight(42);
    cardLayout->addWidget(usernameEdit_);

    cardLayout->addSpacing(12);

    // Password field with show/hide toggle button.
    auto *passwordLayout = new QHBoxLayout();
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(0);

    passwordEdit_ = new QLineEdit(card);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit_->setMaxLength(255);
    passwordEdit_->setStyleSheet(QString::fromLatin1(kInputStyle));
    passwordEdit_->setMinimumHeight(42);
    passwordLayout->addWidget(passwordEdit_);

    passwordToggle_ = new QPushButton(QStringLiteral("显示"), card);
    passwordToggle_->setCursor(Qt::PointingHandCursor);
    passwordToggle_->setFixedHeight(42);
    passwordToggle_->setStyleSheet(QString::fromLatin1(kToggleStyle));
    passwordLayout->addWidget(passwordToggle_);

    cardLayout->addLayout(passwordLayout);

    // ── Error label ──────────────────────────────────────────────────────
    cardLayout->addSpacing(8);
    errorLabel_ = new QLabel(card);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral(
        "color: #f56c6c; font-size: 12px; padding-left: 2px;"));
    errorLabel_->setVisible(false);
    cardLayout->addWidget(errorLabel_);

    cardLayout->addSpacing(10);

    // ── Login button ─────────────────────────────────────────────────────
    loginButton_ = new QPushButton(QStringLiteral("登 录"), card);
    loginButton_->setCursor(Qt::PointingHandCursor);
    loginButton_->setMinimumHeight(44);
    loginButton_->setStyleSheet(QString::fromLatin1(kLoginButtonStyle));
    cardLayout->addWidget(loginButton_);

    cardLayout->addSpacing(16);

    // ── Hint ─────────────────────────────────────────────────────────────
    hintLabel_ = new QLabel(card);
    hintLabel_->setText(QStringLiteral(
        "默认账号：<b>admin</b>　密码：<b>admin123</b>"));
    hintLabel_->setAlignment(Qt::AlignCenter);
    hintLabel_->setTextFormat(Qt::RichText);
    hintLabel_->setStyleSheet(QStringLiteral(
        "color: #c0c4cc; font-size: 12px;"));
    cardLayout->addWidget(hintLabel_);

    cardLayout->addStretch();

    root->addWidget(card, 0, Qt::AlignCenter);

    // ── Connections ───────────────────────────────────────────────────────
    connect(loginButton_, &QPushButton::clicked,
            this, &AdminLoginDialog::onLoginClicked);
    connect(passwordToggle_, &QPushButton::clicked, this, [this]() {
        passwordVisible_ = !passwordVisible_;
        passwordEdit_->setEchoMode(passwordVisible_
            ? QLineEdit::Normal : QLineEdit::Password);
        passwordToggle_->setText(passwordVisible_
            ? QStringLiteral("隐藏") : QStringLiteral("显示"));
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

void AdminLoginDialog::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dark tech-style gradient background.
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor("#1a1a2e"));
    gradient.setColorAt(0.5, QColor("#16213e"));
    gradient.setColorAt(1.0, QColor("#0f3460"));
    painter.fillRect(rect(), gradient);

    // Subtle decorative circles (top-right and bottom-left).
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(64, 158, 255, 25));
    painter.drawEllipse(QPoint(width() - 60, 80), 120, 120);
    painter.setBrush(QColor(64, 158, 255, 15));
    painter.drawEllipse(QPoint(40, height() - 40), 90, 90);
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
                                  : QStringLiteral("登 录"));
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
