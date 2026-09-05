#pragma once

#include <QWidget>

class QComboBox;
class QJsonArray;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ev {

class AdminApiClient;

// UML-038/039: full pile list with station/status filters and remote restart
// for fault piles.
class AdminPilePage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminPilePage(AdminApiClient *api, QWidget *parent = nullptr);

    // Reload station filter options, then the pile list.
    void reloadAll();
    // Reload the pile list with the current filters only.
    void reload();

private:
    void loadStations();
    void loadPiles();
    void fillTable(const QJsonArray &piles);
    void requestRestart(long long pileId, const QString &pileNumber);
    void setStatusText(const QString &text, bool isError);

    AdminApiClient *api_ = nullptr;

    QComboBox *stationBox_ = nullptr;
    QComboBox *statusBox_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};

} // namespace ev
