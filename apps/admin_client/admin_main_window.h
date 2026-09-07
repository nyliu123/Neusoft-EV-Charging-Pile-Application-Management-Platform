#pragma once

#include "admin_api_client.h"
#include "admin_session.h"
#include "network/platform_client.h"

#include <QMainWindow>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace ev {

class AdminDashboardPage;
class AdminOrderPage;
class AdminPilePage;
class AdminStationDetailPage;
class AdminStationPage;
class AdminUserPage;

// Shell window: left navigation, stacked pages, connection status and the
// logout / session-expired re-login flow (UML-034). The station detail page
// (UML-041) lives in the stack but not in the navigation.
class AdminMainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit AdminMainWindow(PlatformClient *client, const AdminSession &session,
                             QWidget *parent = nullptr);

private slots:
    void onNavigationChanged(int index);
    void showLoginAgain(const QString &reason);

private:
    void setupUi();
    void setupConnections();
    void applySession(const AdminSession &session);
    void refreshCurrentPage();

    PlatformClient *client_ = nullptr;
    AdminApiClient api_;
    AdminSession session_;
    bool reloginActive_ = false;

    QListWidget *nav_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QLabel *userLabel_ = nullptr;

    AdminDashboardPage *dashboardPage_ = nullptr;
    AdminPilePage *pilePage_ = nullptr;
    AdminStationPage *stationPage_ = nullptr;
    AdminUserPage *userPage_ = nullptr;
    AdminOrderPage *orderPage_ = nullptr;
    AdminStationDetailPage *stationDetailPage_ = nullptr;
};

} // namespace ev
