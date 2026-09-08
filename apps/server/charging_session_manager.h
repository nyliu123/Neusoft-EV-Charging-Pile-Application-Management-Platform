#pragma once

#include "data/order_repository.h"
#include "services/charge_service.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QTcpSocket>
#include <QTimer>

namespace ev {

// Server-driven charge simulation (UML-029): one 1 Hz timer on the server's
// main thread computes kwh / fee / progress per active order from the order's
// start_time and pushes ChargeUpdate (0x32) frames to the bound client socket.
// The event loop is used instead of a worker thread because QTcpSocket is not
// thread-safe and must be written from its owning thread; the wall-clock
// formula keeps values correct across pushes, so no state is lost this way.
class ChargingSessionManager final : public QObject {
    Q_OBJECT

public:
    explicit ChargingSessionManager(QObject *parent = nullptr);

    void setDatabase(const QSqlDatabase &database);

    // Starts the simulation session for a charging order, or re-binds an
    // existing one to a new socket (e.g. after reconnect via check_pending).
    void ensureSession(const OrderRecord &order, double powerKw, QTcpSocket *socket);
    void stopSession(qint64 orderId);
    // Called on socket disconnect: drops the socket binding but keeps the
    // session; the order keeps charging by wall-clock time (UML-029) and
    // pushes resume when the client re-attaches.
    void detachSocket(QTcpSocket *socket);

private:
    struct Session {
        double powerKw = 0.0;
        double pricePerKwh = 0.0;
        int discountBps = 10000;
        QDateTime startTime;
        QPointer<QTcpSocket> socket;
    };

    void tick();

    QTimer timer_;
    QHash<qint64, Session> sessions_;
    QSqlDatabase database_;
    OrderRepository orderRepository_;
};

} // namespace ev
