#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("evcs_user_client"));
    QApplication::setApplicationVersion(QStringLiteral("1.1.0"));

    evcs::userclient::MainWindow window;
    window.show();
    return app.exec();
}
