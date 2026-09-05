#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class UserHomeWidget final : public QWidget {
    Q_OBJECT

public:
    explicit UserHomeWidget(QWidget *parent = nullptr);
    void refresh();
    void showWelcome(bool isNewUser);
    void setLogoutInProgress(bool inProgress);

signals:
    void logoutRequested();

private:
    QLabel *successMessage_ = nullptr;
    QLabel *avatarLabel_ = nullptr;
    QLabel *nicknameLabel_ = nullptr;
    QLabel *balanceLabel_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
};
