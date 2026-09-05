#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ev_user_client"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("汽车充电管理平台 用户端"));
    window.resize(1000, 680);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("汽车充电管理平台"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    layout->addWidget(new QLabel(
        QStringLiteral("用户端主干已就绪：登录、找站、充电、会员与咨询页面将在功能分支接入。"),
        central));
    layout->addStretch();
    window.setCentralWidget(central);
    window.statusBar()->showMessage(QStringLiteral("未连接服务端"));
    window.show();
    return application.exec();
}

