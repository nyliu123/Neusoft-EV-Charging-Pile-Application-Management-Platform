#include "admin_main_window.h"
#include "client_ui/rhine_widgets.h"

#include "admin_dashboard_page.h"
#include "admin_login_dialog.h"
#include "admin_order_page.h"
#include "admin_pile_page.h"
#include "admin_station_detail_page.h"
#include "admin_station_page.h"
#include "admin_user_page.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace ev {

AdminMainWindow::AdminMainWindow(PlatformClient *client, const AdminSession &session,
                                 QWidget *parent)
    : QMainWindow(parent), client_(client), api_(client, this)
{
    setupUi();
    applySession(session);
    setupConnections();
    refreshCurrentPage();
}

void AdminMainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("汽车充电管理平台 管理端"));
    resize(1280, 800);
    setMinimumSize(1100, 680);

    auto *central = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(central);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Persistent laboratory-style navigation rail.
    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("terminalSidebar"));
    sidebar->setFixedWidth(254);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(14, 24, 14, 10);
    sidebarLayout->setSpacing(20);
    sidebarLayout->addWidget(makeRhineBrand(sidebar, true));
    auto *workspaceLabel = new QLabel(QStringLiteral("运营工作空间 / OPERATIONS"), sidebar);
    workspaceLabel->setProperty("uiClass", "eyebrow");
    sidebarLayout->addWidget(workspaceLabel);
    nav_ = new QListWidget(sidebar);
    nav_->setObjectName(QStringLiteral("sidebarNavigation"));
    nav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_->addItem(QStringLiteral("01    经营看板"));
    nav_->addItem(QStringLiteral("02    充电桩管理"));
    nav_->addItem(QStringLiteral("03    充电站管理"));
    nav_->addItem(QStringLiteral("04    用户管理"));
    nav_->addItem(QStringLiteral("05    订单管理"));
    sidebarLayout->addWidget(nav_, 1);
    auto *railFooter = new QLabel(QStringLiteral("ENERGY FOR TOMORROW.\n电动汽车充电桩应用管理平台"), sidebar);
    railFooter->setObjectName(QStringLiteral("terminalSidebarFooter"));
    railFooter->setWordWrap(true);
    sidebarLayout->addWidget(railFooter);
    bodyLayout->addWidget(sidebar);

    // Right side: header + page stack.
    auto *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(20, 14, 20, 16);
    rightLayout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout();
    auto *headerTitle = new QLabel(QStringLiteral("运营控制中心  /  CONTROL CENTER"), central);
    headerTitle->setProperty("uiClass", "eyebrow");
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch();

    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), central);
    logoutButton_->setProperty("uiClass", "danger");
    logoutButton_->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(logoutButton_);
    rightLayout->addLayout(headerLayout);

    stack_ = new QStackedWidget(central);
    rightLayout->addWidget(stack_, 1);
    bodyLayout->addLayout(rightLayout, 1);

    setCentralWidget(central);

    dashboardPage_ = new AdminDashboardPage(&api_, this);
    connect(dashboardPage_, &AdminDashboardPage::pileManagementRequested,
            this, [this] { nav_->setCurrentRow(1); });
    stack_->addWidget(dashboardPage_);

    pilePage_ = new AdminPilePage(&api_, this);
    stack_->addWidget(pilePage_);

    stationPage_ = new AdminStationPage(&api_, this);
    stack_->addWidget(stationPage_);

    userPage_ = new AdminUserPage(&api_, this);
    stack_->addWidget(userPage_);

    orderPage_ = new AdminOrderPage(&api_, this);
    stack_->addWidget(orderPage_);

    // Station detail (UML-041): stacked but not present in the navigation.
    stationDetailPage_ = new AdminStationDetailPage(&api_, this);
    connect(stationPage_, &AdminStationPage::detailRequested,
            this, [this](long long stationId) {
                stationDetailPage_->loadStation(stationId);
                stack_->setCurrentWidget(stationDetailPage_);
            });
    connect(stationDetailPage_, &AdminStationDetailPage::backRequested, this, [this] {
        stack_->setCurrentWidget(stationPage_);
        stationPage_->reload();
    });
    stack_->addWidget(stationDetailPage_);

    stack_->setCurrentIndex(0);
    nav_->setCurrentRow(0);

    // Status bar.
    connectionLabel_ = new QLabel(client_->state() == PlatformClient::State::Ready
        ? QStringLiteral("连接状态：服务端可用") : QStringLiteral("连接状态：尚未就绪"), this);
    statusBar()->addPermanentWidget(connectionLabel_);
    userLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(userLabel_);
}

void AdminMainWindow::setupConnections()
{
    connect(nav_, &QListWidget::currentRowChanged,
            this, &AdminMainWindow::onNavigationChanged);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(this, QStringLiteral("退出登录"),
            QStringLiteral("确定退出当前管理员账号？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        hide();
        connect(client_, &PlatformClient::logoutFinished, this,
            [this](bool, const QString &) {
                QTimer::singleShot(0, this,
                    [this] { showLoginAgain(QString()); });
            },
            Qt::SingleShotConnection);
        client_->logout();
    });
    connect(&api_, &AdminApiClient::sessionExpired, this,
            &AdminMainWindow::showLoginAgain, Qt::QueuedConnection);
    connect(client_, &PlatformClient::sessionExpired, this,
            &AdminMainWindow::showLoginAgain, Qt::QueuedConnection);
    connect(client_, &PlatformClient::stateChanged, this,
            [this](PlatformClient::State state, const QString &detail) {
                connectionLabel_->setText(QStringLiteral("连接状态：%1").arg(detail));
                statusBar()->showMessage(
                    state == PlatformClient::State::Ready
                        ? QStringLiteral("服务端可用") : detail);
            });
}

void AdminMainWindow::applySession(const AdminSession &session)
{
    session_ = session;
    api_.setSession(session);
    client_->activateSession(session.sessionId);
    setWindowTitle(QStringLiteral("汽车充电管理平台 管理端 - %1").arg(session.username));
    userLabel_->setText(QStringLiteral("管理员：%1").arg(session.username));
    statusBar()->showMessage(QStringLiteral("登录成功"), 3000);
}

void AdminMainWindow::onNavigationChanged(int index)
{
    stack_->setCurrentIndex(index);
    refreshCurrentPage();
}

void AdminMainWindow::refreshCurrentPage()
{
    if (!api_.hasSession()) {
        return;
    }
    QWidget *current = stack_->currentWidget();
    if (current == dashboardPage_) {
        dashboardPage_->reload();
    } else if (current == pilePage_) {
        pilePage_->reloadAll();
    } else if (current == stationPage_) {
        stationPage_->reload();
    } else if (current == userPage_) {
        userPage_->reload();
    } else if (current == orderPage_) {
        orderPage_->reloadAll();
    } else if (current == stationDetailPage_) {
        stationDetailPage_->reload();
    }
}

void AdminMainWindow::showLoginAgain(const QString &reason)
{
    Q_UNUSED(reason)
    if (reloginActive_) {
        return;
    }
    reloginActive_ = true;
    api_.setSession(AdminSession {});
    hide();

    AdminLoginDialog dialog(client_);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    reloginActive_ = false;

    if (!accepted) {
        QCoreApplication::quit();
        return;
    }
    applySession(dialog.session());
    show();
    raise();
    activateWindow();
    refreshCurrentPage();
}

} // namespace ev
