#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class ChargeFlowWidget;
class StationSearchWidget;
class UserInfoWidget;
class OrderListWidget;
class QTabWidget;

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
    ChargeFlowWidget *chargeFlowWidget_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    OrderListWidget *orderListWidget_ = nullptr;
    StationSearchWidget *stationSearchWidget_ = nullptr;
    UserInfoWidget *userInfoWidget_ = nullptr;
};
