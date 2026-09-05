#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_admin_client"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("汽车充电管理平台 管理端"));
    window.resize(1100, 720);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("运营管理中心"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    layout->addWidget(new QLabel(
        QStringLiteral("管理端主干已就绪：经营看板、站点、设备、用户和订单页面将在功能分支接入。"),
        central));
    layout->addStretch();
    window.setCentralWidget(central);
    window.statusBar()->showMessage(QStringLiteral("未连接服务端"));
    window.show();
    return application.exec();
}

