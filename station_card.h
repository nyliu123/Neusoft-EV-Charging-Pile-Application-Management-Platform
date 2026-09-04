#pragma once

#include <QFrame>
#include <QJsonObject>

namespace evcs::userclient {

class StationCard final : public QFrame
{
    Q_OBJECT

public:
    explicit StationCard(const QJsonObject &station, QWidget *parent = nullptr);

signals:
    void stationSelected(qint64 stationId);
    void navigationRequested(qint64 stationId);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 stationId_ = 0;
};

} // namespace evcs::userclient
