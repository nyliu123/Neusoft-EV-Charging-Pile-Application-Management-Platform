#pragma once

#include "apiclient.h"

#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;

namespace evcs::userclient {

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *createLoginPage();
    QWidget *createStationPage();
    QWidget *createReservationPage();
    QWidget *createChargingPage();
    QWidget *createOrderPage();
    QWidget *createProfilePage();
    void connectSignals();
    void connectServer();
    void login();
    void registerUser();
    void updateLoginMode();
    void refreshStations();
    void geocodeLocation();
    void setCurrentLocation(double latitude, double longitude,
                            const QString &description, const QString &source);
    void navigateSelectedStation(const QString &travelMode);
    void loadSelectedStation();
    void reserveSelectedCharger();
    void startSelectedCharger();
    void refreshReservations();
    void cancelSelectedReservation();
    void startSelectedReservation();
    void refreshChargingStatus();
    void stopCharging();
    void refreshOrders();
    void showSelectedOrder();
    void refreshProfile();
    void editDisplayName();
    void chooseAvatar();
    void rechargeWallet();
    void logout();
    qint64 selectedId(QTableWidget *table) const;
    void handleResponse(const QString &action,
                        bool ok,
                        const QJsonObject &data,
                        const QString &errorCode,
                        const QString &errorMessage);
    void populateStations(const QJsonObject &data);
    void populateChargers(const QJsonObject &data);
    void populateReservations(const QJsonObject &data);
    void populateCharging(const QJsonObject &data);
    void populateOrders(const QJsonObject &data);
    void populateProfile(const QJsonObject &data);

    ApiClient apiClient_;
    QTimer chargingTimer_;
    qint64 activeSessionId_ = 0;
    bool hasCurrentLocation_ = false;
    double currentLatitude_ = 0.0;
    double currentLongitude_ = 0.0;
    QNetworkAccessManager *mapNetwork_ = nullptr;

    QStackedWidget *stack_ = nullptr;
    QWidget *loginPage_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QComboBox *loginModeCombo_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QLineEdit *stationKeywordEdit_ = nullptr;
    QLineEdit *stationRegionEdit_ = nullptr;
    QLineEdit *locationEdit_ = nullptr;
    QLabel *locationStatusLabel_ = nullptr;
    QCheckBox *onlyAvailableCheck_ = nullptr;
    QTableWidget *stationTable_ = nullptr;
    QTableWidget *chargerTable_ = nullptr;
    QTableWidget *reservationTable_ = nullptr;
    QLabel *chargingStationLabel_ = nullptr;
    QLabel *chargingChargerLabel_ = nullptr;
    QLabel *chargingTimeLabel_ = nullptr;
    QLabel *chargingEnergyLabel_ = nullptr;
    QLabel *chargingAmountLabel_ = nullptr;
    QPushButton *stopChargingButton_ = nullptr;
    QTableWidget *orderTable_ = nullptr;
    QLabel *profileAvatarLabel_ = nullptr;
    QLabel *profileUsernameLabel_ = nullptr;
    QLabel *profileDisplayNameLabel_ = nullptr;
    QLabel *profilePhoneLabel_ = nullptr;
    QLabel *profileBalanceLabel_ = nullptr;
    QLabel *profileCreatedAtLabel_ = nullptr;
};

} // namespace evcs::userclient
