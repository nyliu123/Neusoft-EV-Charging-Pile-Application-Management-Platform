#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class UserInfoWidget;
class StationListWidget;

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
    void navigationRequested(const QJsonObject &station);
    void pileSelected(const QJsonObject &station, const QJsonObject &pile);

private:
    QLabel *successMessage_ = nullptr;
    UserInfoWidget *userInfoWidget_ = nullptr;
    StationListWidget *stationListWidget_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
};
