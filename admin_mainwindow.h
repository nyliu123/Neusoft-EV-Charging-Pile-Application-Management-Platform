#pragma once

#include "apiclient.h"

#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

class QLabel;
class QChartView;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;

namespace evcs::adminclient {

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *createLoginPage();
    QWidget *createDashboardPage();
    QWidget *createStationPage();
    QWidget *createChargerPage();
    QWidget *createUserPage();
    QWidget *createOrderPage();
    QWidget *createReservationPage();
    QWidget *createSessionPage();
    QWidget *createTariffPage();
    QWidget *createFaultPage();
    void connectSignals();
    void connectServer();
    void login();
    void refreshAll();
    void refreshDashboard();
    void refreshStations();
    void refreshChargers();
    void refreshUsers();
    void refreshOrders();
    void refreshReservations();
    void refreshSessions();
    void refreshTariffs();
    void refreshFaults();
    void updateTrendDisplay();
    void showSelectedStationDevices();
    void showAllChargers();
    void editStation(bool createNew);
    void editCharger(bool createNew);
    void setChargerStatus();
    void restartCharger();
    void showChargerOperations();
    void setUserStatus();
    void editTariff(bool createNew);
    void editFault(bool createNew);
    qint64 selectedId(QTableWidget *table) const;
    void handleResponse(const QString &action,
                        bool ok,
                        const QJsonObject &data,
                        const QString &errorCode,
                        const QString &errorMessage);
    void populateDashboard(const QJsonObject &data);
    void populateStations(const QJsonObject &data);
    void populateChargers(const QJsonObject &data);
    void populateUsers(const QJsonObject &data);
    void populateOrders(const QJsonObject &data);
    void populateReservations(const QJsonObject &data);
    void populateSessions(const QJsonObject &data);
    void populateTariffs(const QJsonObject &data);
    void populateFaults(const QJsonObject &data);

    ApiClient apiClient_;
    QStackedWidget *stack_ = nullptr;
    QWidget *loginPage_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;

    QLabel *userCountLabel_ = nullptr;
    QLabel *stationCountLabel_ = nullptr;
    QLabel *chargerCountLabel_ = nullptr;
    QLabel *chargerStateLabel_ = nullptr;
    QLabel *todayLabel_ = nullptr;
    QLabel *revenueLabel_ = nullptr;
    QTableWidget *trendTable_ = nullptr;
    QComboBox *trendRangeCombo_ = nullptr;
    QChartView *trendChart_ = nullptr;
    QChartView *statusChart_ = nullptr;
    QJsonObject dashboardData_;
    QTableWidget *stationTable_ = nullptr;
    qint64 chargerStationFilterId_ = 0;
    QLabel *chargerFilterLabel_ = nullptr;
    QComboBox *chargerStationFilterCombo_ = nullptr;
    QComboBox *chargerStatusFilterCombo_ = nullptr;
    QTableWidget *chargerTable_ = nullptr;
    QLineEdit *userPhoneFilterEdit_ = nullptr;
    QTableWidget *userTable_ = nullptr;
    QLineEdit *orderNoFilterEdit_ = nullptr;
    QLineEdit *orderPhoneFilterEdit_ = nullptr;
    QLineEdit *orderStationFilterEdit_ = nullptr;
    QComboBox *orderStationFilterCombo_ = nullptr;
    QLineEdit *orderChargerFilterEdit_ = nullptr;
    QComboBox *orderStatusFilterCombo_ = nullptr;
    QCheckBox *orderDateFilterCheck_ = nullptr;
    QDateEdit *orderStartDateEdit_ = nullptr;
    QDateEdit *orderEndDateEdit_ = nullptr;
    QComboBox *orderTimeRangeCombo_ = nullptr;
    QTableWidget *orderTable_ = nullptr;
    QTableWidget *reservationTable_ = nullptr;
    QTableWidget *sessionTable_ = nullptr;
    QTableWidget *tariffTable_ = nullptr;
    QTableWidget *faultTable_ = nullptr;
    QTimer userSearchDebounce_;
    QPointer<QDoubleSpinBox> pendingLongitude_;
    QPointer<QDoubleSpinBox> pendingLatitude_;
};

} // namespace evcs::adminclient
