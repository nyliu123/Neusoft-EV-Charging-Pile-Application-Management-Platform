#include "charging_session_manager.h"

#include "common/protocol.h"
#include "network/frame_codec.h"

#include <QDebug>
#include <QJsonObject>

namespace ev {

ChargingSessionManager::ChargingSessionManager(QObject *parent)
    : QObject(parent)
{
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &ChargingSessionManager::tick);
}

void ChargingSessionManager::setDatabase(const QSqlDatabase &database)
{
    database_ = database;
}

void ChargingSessionManager::ensureSession(const OrderRecord &order, double powerKw,
                                           QTcpSocket *socket)
{
    Session session;
    session.powerKw = powerKw;
    session.pricePerKwh = order.pricePerKwh;
    session.discountBps = order.discountBps;
    session.startTime = ChargeService::parseDbDateTime(order.startTime);
    session.socket = socket;
    sessions_.insert(order.orderId, session);
    if (!timer_.isActive()) {
        timer_.start();
    }
    qInfo().noquote() << "charge simulation started for order" << order.orderId;
}

void ChargingSessionManager::stopSession(qint64 orderId)
{
    if (sessions_.remove(orderId) > 0) {
        qInfo().noquote() << "charge simulation stopped for order" << orderId;
    }
    if (sessions_.isEmpty()) {
        timer_.stop();
    }
}

void ChargingSessionManager::detachSocket(QTcpSocket *socket)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it.value().socket == socket) {
            // Keep the session: the order keeps charging by wall-clock time
            // and pushes resume on the next check_pending re-attach.
            it.value().socket = nullptr;
        }
    }
}

void ChargingSessionManager::tick()
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        const qint64 orderId = it.key();
        const Session &session = it.value();

        const auto snapshot = ChargeService::computeChargeData(
            session.startTime, session.powerKw, session.pricePerKwh, session.discountBps);
        if (!snapshot.success) {
            qWarning().noquote() << "charge simulation compute failed for order"
                                 << orderId << ":" << snapshot.message;
            continue;
        }

        // Persist intermediate values so the admin order page shows live
        // data for charging orders (optional write per UML-029).
        const auto stored = orderRepository_.updateChargeData(
            database_, orderId, snapshot.data.kwh, snapshot.data.feeCent, snapshot.data.grossFeeCent);
        if (!stored.success) {
            qWarning().noquote() << "charge data persist failed for order"
                                 << orderId << ":" << stored.message;
        }

        if (session.socket == nullptr || session.socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        const QJsonObject payload {
            {QStringLiteral("protocol_version"), static_cast<qint64>(ProtocolVersion)},
            {QStringLiteral("order_id"), orderId},
            {QStringLiteral("charge_amount_kwh"), snapshot.data.kwh},
            {QStringLiteral("current_fee_cent"), snapshot.data.feeCent},
            {"gross_fee_cent",snapshot.data.grossFeeCent},
            {"discount_fee_cent",snapshot.data.grossFeeCent-snapshot.data.feeCent},
            {QStringLiteral("progress"), snapshot.data.progressPercent}
        };
        session.socket->write(FrameCodec::encode(
            static_cast<quint32>(MessageType::ChargeUpdate), payload));
    }
}

} // namespace ev
