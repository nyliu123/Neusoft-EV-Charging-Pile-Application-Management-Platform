#pragma once

#include "admin_session.h"

#include <QDialog>
#include <QString>

class QPaintEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace ev {

class PlatformClient;

// Admin login dialog (UML-034).
//
// Apple-inspired split login surface, shared with the user client.
//
// Implements the full validation flow:
//   1. Non-empty check for username and password
//   2. Length check (username 1-50, password 1-255)
//   3. Disable button + show "logging in..." during async request
//   4. On success: save session and accept
//   5. On failure: show "用户名或密码错误", clear password, keep username
class AdminLoginDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AdminLoginDialog(PlatformClient *client, QWidget *parent = nullptr);

    // Returns the session after successful login.
    // Call exec() first, then session() if result is Accepted.
    AdminSession session() const { return session_; }

private slots:
    void onLoginClicked();
    void onLoginResponse(bool success, const QString &message, const AdminSession &session);

private:
    void setLoading(bool loading);
    void showError(const QString &message);
    void clearError();
    bool validateInput(const QString &username, const QString &password);

    PlatformClient *client_ = nullptr;
    AdminSession session_;

    QLabel *titleLabel_ = nullptr;
    QLabel *subTitleLabel_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QToolButton *passwordToggle_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QString pendingRequestId_;
    bool passwordVisible_ = false;
};

} // namespace ev
