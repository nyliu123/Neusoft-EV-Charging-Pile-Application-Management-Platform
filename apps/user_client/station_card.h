#pragma once

#include <QFrame>
#include <QJsonObject>

class QMouseEvent;

class StationCard final : public QFrame {
    Q_OBJECT

public:
    explicit StationCard(QJsonObject station, QWidget *parent = nullptr);

signals:
    void stationSelected(const QJsonObject &station);
    void navigationRequested(const QJsonObject &station);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QJsonObject station_;
};
