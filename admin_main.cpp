#include "admin_mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("BitEVCS"));
    QApplication::setApplicationName(QStringLiteral("evcs_admin_client"));
    QApplication::setApplicationVersion(QStringLiteral("1.2.0"));

    evcs::adminclient::MainWindow window;
    window.show();
    return app.exec();
}
