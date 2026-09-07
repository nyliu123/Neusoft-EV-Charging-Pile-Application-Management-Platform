#pragma once

#include <QWidget>

class QJsonArray;
class QJsonObject;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ev {

class AdminApiClient;

// UML-041: single-station device detail — station header info plus the
// station's piles with the remote-restart action reused from UML-039.
// Not part of the left navigation; shown via detailRequested() from the
// station list page and returns through backRequested().
class AdminStationDetailPage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminStationDetailPage(AdminApiClient *api, QWidget *parent = nullptr);

    void loadStation(long long stationId);
    void reload();

signals:
    void backRequested();

private:
    void applyStation(const QJsonObject &station, const QJsonArray &piles);
    void fillTable(const QJsonArray &piles);
    void requestRestart(long long pileId, const QString &pileNumber, int row);
    void setLoading(bool loading, const QString &message);
    void setStatusText(const QString &text, bool isError);

    AdminApiClient *api_ = nullptr;
    long long stationId_ = 0;
    QLabel *infoLabel_ = nullptr;
    QPushButton *backButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *loadingOverlay_ = nullptr;
};

} // namespace ev
