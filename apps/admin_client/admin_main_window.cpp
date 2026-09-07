#include "admin_main_window.h"

#include "admin_dashboard_page.h"
#include "admin_login_dialog.h"
#include "admin_pile_page.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
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
    resize(1200, 760);

    auto *central = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(central);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left navigation.
    nav_ = new QListWidget(central);
    nav_->setObjectName(QStringLiteral("sidebarNavigation"));
    nav_->setFixedWidth(190);
    nav_->addItem(QStringLiteral("经营看板"));
    nav_->addItem(QStringLiteral("充电桩管理"));
    bodyLayout->addWidget(nav_);

    // Right side: header + page stack.
    auto *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(20, 14, 20, 16);
    rightLayout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout();
    auto *headerTitle = new QLabel(QStringLiteral("运营管理端"), central);
    headerTitle->setProperty("uiClass", "sectionTitle");
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

    stack_->setCurrentIndex(0);

    // Status bar.
    connectionLabel_ = new QLabel(QStringLiteral("连接状态：连接中"), this);
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
        client_->logout();
        showLoginAgain(QStringLiteral("已退出登录"));
    });
    connect(&api_, &AdminApiClient::sessionExpired, this,
            &AdminMainWindow::showLoginAgain);
    connect(client_, &PlatformClient::sessionExpired, this,
            &AdminMainWindow::showLoginAgain);
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
    const int index = stack_->currentIndex();
    if (index == 0) {
        dashboardPage_->reload();
    } else if (index == 1) {
        pilePage_->reloadAll();
    }
}

void AdminMainWindow::showLoginAgain(const QString &reason)
{
    if (reloginActive_) {
        return;
    }
    reloginActive_ = true;
    api_.setSession(AdminSession {});

    if (!reason.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), reason);
    }

    AdminLoginDialog dialog(client_, this);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    reloginActive_ = false;

    if (!accepted) {
        QCoreApplication::quit();
        return;
    }
    applySession(dialog.session());
    refreshCurrentPage();
}

} // namespace ev
