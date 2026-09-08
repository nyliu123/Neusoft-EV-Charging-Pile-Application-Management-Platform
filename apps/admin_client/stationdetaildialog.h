#ifndef STATIONDETAILDIALOG_H
#define STATIONDETAILDIALOG_H

#include <QDialog>
#include <QTableView>
#include <QStandardItemModel>
#include <QString>

// 站内设备详情弹窗
class StationDetailDialog : public QDialog {
    Q_OBJECT
public:
    explicit StationDetailDialog(const QString &stationName, QWidget *parent = nullptr);

private:
    QTableView *m_deviceTable;
    QStandardItemModel *m_deviceModel;
};

#endif // STATIONDETAILDIALOG_H
