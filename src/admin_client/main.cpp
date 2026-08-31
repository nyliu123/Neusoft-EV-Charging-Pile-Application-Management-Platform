#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("BitEVCS"));

    evcs::adminclient::MainWindow window;
    window.show();
    return app.exec();
}
