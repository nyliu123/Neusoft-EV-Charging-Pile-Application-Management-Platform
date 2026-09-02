#include "admin_mainwindow.h"
#include "admin_style.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace evcs::adminclient {

// 主窗口只负责页面装配、会话入口和全局信号连接。
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电动汽车充电桩应用管理平台 · 运营管理端"));
    resize(1280, 820);
    setStyleSheet(adminStyleSheet());

    stack_ = new QStackedWidget;
    stack_->setObjectName(QStringLiteral("adminStack"));
    loginPage_ = createLoginPage();
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("adminTabs"));
    tabs_->addTab(createDashboardPage(), QStringLiteral("经营概览"));
    tabs_->addTab(createStationPage(), QStringLiteral("站点"));
    tabs_->addTab(createChargerPage(), QStringLiteral("充电桩"));
    tabs_->addTab(createUserPage(), QStringLiteral("用户"));
    tabs_->addTab(createReservationPage(), QStringLiteral("预约"));
    tabs_->addTab(createSessionPage(), QStringLiteral("充电记录"));
    tabs_->addTab(createOrderPage(), QStringLiteral("订单"));
    tabs_->addTab(createTariffPage(), QStringLiteral("价格"));
    tabs_->addTab(createFaultPage(), QStringLiteral("故障"));
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"));
    tabs_->setCornerWidget(logoutButton, Qt::TopRightCorner);
    connect(logoutButton, &QPushButton::clicked, this, [this] {
        apiClient_.sendRequest(QStringLiteral("auth.logout"));
    });
    stack_->addWidget(loginPage_);
    stack_->addWidget(tabs_);
    setCentralWidget(stack_);
    connectSignals();
    QTimer::singleShot(0, this, &MainWindow::connectServer);
}

} // namespace evcs::adminclient
