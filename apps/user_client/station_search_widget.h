#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace ev {
class UserApiClient;
}

class StationSearchWidget final : public QWidget {
    Q_OBJECT

public:
    explicit StationSearchWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refresh();

private:
    void search();
    void loadStations(bool hasLocation = false, double longitude = 0.0,
                      double latitude = 0.0);
    void renderStations(const QJsonObject &result, bool locationAvailable);
    void showStationDetail(const QJsonObject &station);
    void renderStationDetail(const QJsonObject &result);
    void setBusy(bool busy, const QString &message = {});

    ev::UserApiClient *api_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QWidget *listPage_ = nullptr;
    QWidget *detailPage_ = nullptr;
    QComboBox *areaBox_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QPushButton *searchButton_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QVBoxLayout *stationListLayout_ = nullptr;
    QLabel *detailTitle_ = nullptr;
    QLabel *detailMeta_ = nullptr;
    QTableWidget *pileTable_ = nullptr;
};
