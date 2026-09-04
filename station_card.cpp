#include "station_card.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace evcs::userclient {

StationCard::StationCard(const QJsonObject &station, QWidget *parent)
    : QFrame(parent),
      stationId_(static_cast<qint64>(station.value(QStringLiteral("id")).toDouble()))
{
    setCursor(Qt::PointingHandCursor);
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "StationCard { background:#ffffff; border:1px solid #d8e2ea; border-radius:10px; }"
        "StationCard:hover { border-color:#2582d8; background:#f7fbff; }"));
    auto *layout = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    auto *name = new QLabel(QStringLiteral("<b>%1</b>")
        .arg(station.value(QStringLiteral("name")).toString().toHtmlEscaped()));
    auto *distance = new QPushButton(station.contains(QStringLiteral("distanceKm"))
        ? QStringLiteral("%1 km · 导航").arg(station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 2)
        : QStringLiteral("设置位置后导航"));
    distance->setFlat(true);
    distance->setStyleSheet(QStringLiteral("color:#1769aa;text-decoration:underline;"));
    top->addWidget(name);
    top->addStretch();
    top->addWidget(distance);
    layout->addLayout(top);
    layout->addWidget(new QLabel(QStringLiteral("%1 · ¥%2/kWh")
        .arg(station.value(QStringLiteral("address")).toString())
        .arg(station.value(QStringLiteral("minimumPriceCentsPerKwh")).toInt() / 100.0, 0, 'f', 2)));
    const int idle = station.value(QStringLiteral("idleCount")).toInt();
    const int total = station.value(QStringLiteral("chargerCount")).toInt();
    auto *availability = new QLabel(QStringLiteral("空闲 %1 / 总共 %2 · 在线率 %3%")
        .arg(idle).arg(total)
        .arg(station.value(QStringLiteral("onlineRate")).toDouble() * 100.0, 0, 'f', 1));
    availability->setStyleSheet(idle > 0 ? QStringLiteral("color:#157347;")
                                         : QStringLiteral("color:#b42318;"));
    layout->addWidget(availability);
    connect(distance, &QPushButton::clicked, this, [this] { emit navigationRequested(stationId_); });
}

void StationCard::mouseReleaseEvent(QMouseEvent *event)
{
    QFrame::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) emit stationSelected(stationId_);
}

} // namespace evcs::userclient
