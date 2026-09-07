#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace ev {

class AdminApiClient;

// UML-042: create-station form. Coordinates are filled via the shared
// geocode endpoint (StationRequest "geocode") and can also be entered
// manually — the inputs unlock automatically when geocoding fails.
class AdminAddStationDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AdminAddStationDialog(AdminApiClient *api, QWidget *parent = nullptr);

    QString stationName() const;

private slots:
    void fetchCoordinates();

private:
    void setManualCoordinatesEnabled(bool enabled);
    bool validateAndSubmit();

    AdminApiClient *api_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QLineEdit *longitudeEdit_ = nullptr;
    QLineEdit *latitudeEdit_ = nullptr;
    QDoubleSpinBox *priceSpin_ = nullptr;
    QPushButton *geocodeButton_ = nullptr;
    QPushButton *manualButton_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QString stationName_;
};

} // namespace ev
