#include "admin_main_window.h"

#include <QFont>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace ev {

AdminMainWindow::AdminMainWindow(PlatformClient *client, const AdminSession &session,
                                 QWidget *parent)
    : QMainWindow(parent), client_(client), session_(session)
{
    setupUi();
    setupConnections();
}

void AdminMainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("汽车充电管理平台 管理端 - %1")
                       .arg(session_.username));
    resize(1100, 720);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("运营管理中心"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *welcome = new QLabel(
        QStringLiteral("欢迎回来，%1。站点、设备、用户和订单管理模块将在后续功能分支中接入。")
            .arg(session_.username),
        central);
    welcome->setWordWrap(true);
    layout->addWidget(welcome);

    connectionLabel_ = new QLabel(QStringLiteral("连接状态：正在连接"), central);
    layout->addWidget(connectionLabel_);
    layout->addStretch();

    setCentralWidget(central);

    // Status bar.
    userLabel_ = new QLabel(QStringLiteral("管理员：%1").arg(session_.username));
    statusBar()->addPermanentWidget(userLabel_);
    statusBar()->showMessage(QStringLiteral("登录成功"));
}

void AdminMainWindow::setupConnections()
{
    connect(client_, &PlatformClient::stateChanged, this,
            [this](PlatformClient::State state, const QString &detail) {
        connectionLabel_->setText(QStringLiteral("连接状态：%1").arg(detail));
        statusBar()->showMessage(
            state == PlatformClient::State::Ready
                ? QStringLiteral("服务端可用") : detail);
    });
}

} // namespace ev
