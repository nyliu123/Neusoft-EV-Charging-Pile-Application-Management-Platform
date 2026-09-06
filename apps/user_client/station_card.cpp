#include "station_card.h"

#include <QFont>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>

StationCard::StationCard(QJsonObject station, QWidget *parent)
    : QFrame(parent), station_(std::move(station))
{
    setCursor(Qt::PointingHandCursor);
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "StationCard { background: white; border: 1px solid #d8e2ea; border-radius: 10px; }"
        "StationCard:hover { border-color: #2582d8; background: #f7fbff; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);
    auto *top = new QHBoxLayout;
    auto *name = new QLabel(station_.value(QStringLiteral("station_name")).toString(), this);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    nameFont.setPointSize(12);
    name->setFont(nameFont);
    top->addWidget(name);
    top->addStretch();

    auto *distanceButton = new QPushButton(this);
    distanceButton->setFlat(true);
    distanceButton->setCursor(Qt::PointingHandCursor);
    distanceButton->setStyleSheet(QStringLiteral(
        "color: #1769aa; border: none; text-decoration: underline;"));
    if (station_.contains(QStringLiteral("distance_km"))) {
        distanceButton->setText(QStringLiteral("%1 km · 导航")
            .arg(station_.value(QStringLiteral("distance_km")).toDouble(), 0, 'f', 2));
    } else {
        distanceButton->setText(QStringLiteral("距离不可用"));
        distanceButton->setEnabled(false);
    }
    top->addWidget(distanceButton);
    layout->addLayout(top);

    auto *address = new QLabel(station_.value(QStringLiteral("address")).toString(), this);
    address->setWordWrap(true);
    address->setStyleSheet(QStringLiteral("color: #616161;"));
    layout->addWidget(address);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(QStringLiteral("¥%1/度")
        .arg(station_.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2), this));
    bottom->addStretch();
    const int idle = station_.value(QStringLiteral("idle_count")).toInt();
    const int total = station_.value(QStringLiteral("total_piles")).toInt();
    auto *availability = new QLabel(idle > 0
        ? QStringLiteral("空闲 %1 / 总共 %2").arg(idle).arg(total)
        : QStringLiteral("暂无空闲 · 总共 %1").arg(total), this);
    availability->setStyleSheet(idle > 0
        ? QStringLiteral("color: #157347; font-weight: 600;")
        : QStringLiteral("color: #b42318; font-weight: 600;"));
    bottom->addWidget(availability);
    layout->addLayout(bottom);

    connect(distanceButton, &QPushButton::clicked, this, [this] {
        emit navigationRequested(station_);
    });
}

void StationCard::mouseReleaseEvent(QMouseEvent *event)
{
    QFrame::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        emit stationSelected(station_);
    }
}
