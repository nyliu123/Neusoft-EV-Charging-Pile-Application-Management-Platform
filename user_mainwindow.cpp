#include "user_mainwindow.h"
#include "user_style.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtMath>

#include <QtWebEngineWidgets/QWebEngineView>

namespace evcs::userclient {

// 主窗口只负责组装页面、初始化状态和连接公共信号。
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电动汽车充电用户端"));
    resize(1120, 820);
    setStyleSheet(userStyleSheet());
    mapNetwork_ = new QNetworkAccessManager(this);

    stack_ = new QStackedWidget;
    stack_->setObjectName(QStringLiteral("userStack"));
    loginPage_ = createLoginPage();
    stack_->addWidget(loginPage_);

    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("userTabs"));
    tabs_->addTab(createStationPage(), QStringLiteral("附近站点"));
    tabs_->addTab(createReservationPage(), QStringLiteral("我的预约"));
    tabs_->addTab(createChargingPage(), QStringLiteral("正在充电"));
    tabs_->addTab(createOrderPage(), QStringLiteral("历史订单"));
    tabs_->addTab(createProfilePage(), QStringLiteral("个人中心"));
    stack_->addWidget(tabs_);
    setCentralWidget(stack_);

    connectSignals();
    chargingTimer_.setInterval(1000);
    connect(&chargingTimer_, &QTimer::timeout, this, &MainWindow::refreshChargingStatus);
    apiClient_.connectToServer(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
}

} // namespace evcs::userclient
