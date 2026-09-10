#pragma once

#include <QWidget>

class QLabel;
class QDialog;
class QPushButton;

namespace ev {
class UserApiClient;
}

class UserInfoWidget final : public QWidget {
    Q_OBJECT

public:
    explicit UserInfoWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refreshFromSession();
    void setLogoutInProgress(bool inProgress);

signals:
    void logoutRequested();
    void pageRequested(QDialog *page);

private:
    void refreshFromServer();
    void changeAvatar();
    void editNickname();
    void recharge();
    void setBusy(bool busy);

    ev::UserApiClient *api_ = nullptr;
    QLabel *avatarLabel_ = nullptr;
    QLabel *nicknameLabel_ = nullptr;
    QLabel *userIdLabel_ = nullptr;
    QLabel *balanceLabel_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *changeAvatarButton_ = nullptr;
    QPushButton *editNicknameButton_ = nullptr;
    QPushButton *rechargeButton_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
};
