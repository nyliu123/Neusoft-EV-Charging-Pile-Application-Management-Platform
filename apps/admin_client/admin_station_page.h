#pragma once

#include <QWidget>

class QJsonArray;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ev {

class AdminApiClient;

// UML-040: station list with pile counts, "查看详情" per row and the
// "新增充电站" entry point (UML-042 dialog).
class AdminStationPage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminStationPage(AdminApiClient *api, QWidget *parent = nullptr);

    void reload();

signals:
    void detailRequested(long long stationId);

private:
    void loadStations();
    void fillTable(const QJsonArray &stations);
    void openAddDialog();
    void setLoading(bool loading, const QString &message);
    void setStatusText(const QString &text, bool isError);

    AdminApiClient *api_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *loadingOverlay_ = nullptr;
};

} // namespace ev
