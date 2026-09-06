#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class UserInfoWidget;

namespace ev {
class UserApiClient;
}

class UserHomeWidget final : public QWidget {
    Q_OBJECT

public:
    explicit UserHomeWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refresh();
    void showWelcome(bool isNewUser);
    void setLogoutInProgress(bool inProgress);

signals:
    void logoutRequested();

private:
    QLabel *successMessage_ = nullptr;
    UserInfoWidget *userInfoWidget_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
};
