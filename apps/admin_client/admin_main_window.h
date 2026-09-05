#pragma once

#include "admin_session.h"
#include "network/platform_client.h"

#include <QMainWindow>
#include <QString>

class QLabel;

namespace ev {

class AdminMainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit AdminMainWindow(PlatformClient *client, const AdminSession &session,
                             QWidget *parent = nullptr);

private:
    void setupUi();
    void setupConnections();

    PlatformClient *client_ = nullptr;
    AdminSession session_;

    QLabel *connectionLabel_ = nullptr;
    QLabel *userLabel_ = nullptr;
};

} // namespace ev
