#pragma once

#include <QWidget>

class QComboBox;
class QDateEdit;
class QJsonArray;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ev {

class AdminApiClient;

// UML-046: full order list with combinable filters (status, station, date
// range) and a reset action. Read-only module — orders are driven by the
// user-side charging flow, the admin client only monitors them.
class AdminOrderPage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminOrderPage(AdminApiClient *api, QWidget *parent = nullptr);

    void reloadAll();
    void reload();

private:
    void loadStations();
    void loadOrders();
    void fillTable(const QJsonArray &orders);
    void resetFilters();
    void applyDatePreset();
    void setLoading(bool loading, const QString &message);
    void setStatusText(const QString &text, bool isError);

    AdminApiClient *api_ = nullptr;
    QComboBox *statusBox_ = nullptr;
    QComboBox *stationBox_ = nullptr;
    QComboBox *datePresetBox_ = nullptr;
    QDateEdit *startDateEdit_ = nullptr;
    QDateEdit *endDateEdit_ = nullptr;
    QPushButton *queryButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *loadingOverlay_ = nullptr;
};

} // namespace ev
