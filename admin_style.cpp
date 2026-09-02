#include "admin_style.h"

#include "style_loader.h"

QString adminStyleSheet()
{
    return loadStyleResources({
        QStringLiteral(":/qss/admin_common.qss"),
        QStringLiteral(":/qss/admin_login.qss"),
        QStringLiteral(":/qss/admin_dashboard.qss"),
        QStringLiteral(":/qss/admin_station.qss"),
        QStringLiteral(":/qss/admin_charger.qss"),
        QStringLiteral(":/qss/admin_user.qss"),
        QStringLiteral(":/qss/admin_order.qss"),
        QStringLiteral(":/qss/admin_reservation.qss"),
        QStringLiteral(":/qss/admin_session.qss"),
        QStringLiteral(":/qss/admin_tariff.qss"),
        QStringLiteral(":/qss/admin_fault.qss")
    });
}
