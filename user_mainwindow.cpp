#include "user_mainwindow.h"
#include "user_session.h"
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
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 2 && apiClient_.isConnected() && !apiClient_.token().isEmpty()) {
            apiClient_.sendRequest(QStringLiteral("order.pending"));
        }
        if (index == 4 && !apiClient_.token().isEmpty()) {
            const QJsonObject cached = UserSession::instance().user();
            if (!cached.isEmpty()) populateProfile({{QStringLiteral("user"), cached}});
        }
    });
    stack_->addWidget(tabs_);
    setCentralWidget(stack_);

    connectSignals();
    connect(&apiClient_, &ApiClient::eventReceived, this,
            [this](const QString &action, const QJsonObject &data) {
        if (action == QStringLiteral("charging.update")) populateCharging(data);
    });
    chargingTimer_.setInterval(5000);
    connect(&chargingTimer_, &QTimer::timeout, this, &MainWindow::refreshChargingStatus);
    apiClient_.connectToServer(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
}

} // namespace evcs::userclient
