#pragma once

#include "admin_session.h"

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

namespace ev {

class PlatformClient;

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

    PlatformClient *client_ = nullptr;
    AdminSession session_;

    QLabel *titleLabel_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QString pendingRequestId_;
};

} // namespace ev
