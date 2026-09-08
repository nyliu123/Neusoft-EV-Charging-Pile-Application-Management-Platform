#ifndef ADDSTATIONDIALOG_H
#define ADDSTATIONDIALOG_H

#include <QDialog>

namespace Ui {
class AddStationDialog;
}

class AddStationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddStationDialog(QWidget *parent = nullptr);
    ~AddStationDialog();

signals:
    // price is yuan/kWh; this signal does not imply persistence.
    void stationSubmitted(const QString &name, const QString &address,
                          double longitude, double latitude, double price);

private:
    Ui::AddStationDialog *ui;

private slots:
    void on_confirmButton_clicked();

private slots:
    void on_openBigScreenBtn_clicked();
};

#endif // ADDSTATIONDIALOG_H
