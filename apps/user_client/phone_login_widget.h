#pragma once

#include "common/phone_validator.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class PhoneLoginWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PhoneLoginWidget(QWidget *parent = nullptr);
    void setLoginInProgress(bool inProgress);

signals:
    void phoneAccepted(const QString &phone);

private:
    void validatePhone();
    void showValidationError(ev::PhoneValidationError error);

    QLineEdit *phoneInput_ = nullptr;
    QLabel *phoneMessage_ = nullptr;
    QPushButton *loginButton_ = nullptr;
};
