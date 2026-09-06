#pragma once

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QTableWidget;

namespace ev {
class UserApiClient;
}

class StationDetailDialog final : public QDialog {
    Q_OBJECT

public:
    StationDetailDialog(ev::UserApiClient *api, QJsonObject station,
                        QWidget *parent = nullptr);

signals:
    void pileSelected(const QJsonObject &station, const QJsonObject &pile);

private:
    void load();
    void applyResult(const QJsonObject &result);

    ev::UserApiClient *api_ = nullptr;
    QJsonObject station_;
    QLabel *summaryLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QTableWidget *table_ = nullptr;
};
