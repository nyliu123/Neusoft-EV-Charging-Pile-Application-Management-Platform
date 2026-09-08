#pragma once

#include "common/result.h"

#include <QJsonObject>
#include <QSqlDatabase>

namespace ev {

class OrderService final {
public:
    // authenticatedUserId must come from the server's connection-bound session.
    Result<QJsonObject> queryOrders(QSqlDatabase &database, qint64 authenticatedUserId) const;
};

} // namespace ev
