#pragma once

#include <QJsonObject>
#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace ev {
class UserApiClient;
}

class StationListWidget final : public QWidget {
    Q_OBJECT

public:
    explicit StationListWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refresh();

signals:
    void navigationRequested(const QJsonObject &station);
    void pileSelected(const QJsonObject &station, const QJsonObject &pile);

private:
    void resolveLocation();
    void loadStations();
    void loadStations(double longitude, double latitude, const QString &source);
    void showStations(const QJsonObject &result, const QString &source);
    void clearCards();

    ev::UserApiClient *api_ = nullptr;
    QComboBox *presetBox_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QPushButton *searchButton_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QVBoxLayout *cardsLayout_ = nullptr;
    QString pendingWarning_;
};
