#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
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
    void updateNavigationPreview();
    void startNavigation();
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
    QLabel *navigationPreview_ = nullptr;
    QLabel *navigationDistance_ = nullptr;
    QLabel *navigationDuration_ = nullptr;
    QRadioButton *driveMode_ = nullptr;
    QRadioButton *walkMode_ = nullptr;
    QPushButton *startNavigationButton_ = nullptr;
    QTableWidget *pileTable_ = nullptr;
    bool hasOriginLocation_ = false;
    double originLongitude_ = 0.0;
    double originLatitude_ = 0.0;
    bool hasDestinationLocation_ = false;
    double destinationLongitude_ = 0.0;
    double destinationLatitude_ = 0.0;
    QString destinationName_;
};
