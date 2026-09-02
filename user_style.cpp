#include "user_style.h"

#include "style_loader.h"

QString userStyleSheet()
{
    return loadStyleResources({
        QStringLiteral(":/qss/user_common.qss"),
        QStringLiteral(":/qss/user_login.qss"),
        QStringLiteral(":/qss/user_station.qss"),
        QStringLiteral(":/qss/user_reservation.qss"),
        QStringLiteral(":/qss/user_charging.qss"),
        QStringLiteral(":/qss/user_order.qss"),
        QStringLiteral(":/qss/user_profile.qss")
    });
}
