#include "addstationdialog.h"
#include "ui_addstationdialog.h"
#include <QTimer>
#include <QMessageBox>
#include <cmath>

AddStationDialog::AddStationDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AddStationDialog)
{
    ui->setupUi(this);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

AddStationDialog::~AddStationDialog()
{
    delete ui;
}

void AddStationDialog::on_confirmButton_clicked()
{
    const QString name = ui->nameLineEdit->text().trimmed();
    const QString address = ui->addressLineEdit->text().trimmed();
    if (name.isEmpty() || address.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("站名和地址不能为空！"));
        (name.isEmpty() ? ui->nameLineEdit : ui->addressLineEdit)->setFocus();
        return;
    }

    // These controls are QDoubleSpinBox instances despite their historic names.
    // Check the editor before reading value(): invalid edits may retain an old value.
    for (auto *spinBox : {ui->lngLineEdit, ui->latLineEdit, ui->priceSpinBox}) {
        if (!spinBox->hasAcceptableInput()) {
            QMessageBox::warning(this, tr("提示"), tr("请输入有效的经纬度和单价！"));
            spinBox->setFocus();
            return;
        }
        spinBox->interpretText();
    }
    const double longitude = ui->lngLineEdit->value();
    const double latitude = ui->latLineEdit->value();
    const double price = ui->priceSpinBox->value();
    if (!std::isfinite(longitude) || longitude < -180.0 || longitude > 180.0
        || !std::isfinite(latitude) || latitude < -90.0 || latitude > 90.0) {
        QMessageBox::warning(this, tr("提示"),
                             tr("经度须在 -180～180，纬度须在 -90～90 之间！"));
        return;
    }
    if (!std::isfinite(price) || price <= 0.0) {
        QMessageBox::warning(this, tr("提示"), tr("每度电单价必须大于 0！"));
        ui->priceSpinBox->setFocus();
        return;
    }

    // Local values only: the network adapter must use the agreed wire contract.
    // Do not accept() here; only a successful server response confirms saving.

    // ==========================================
        // [NO.90 任务核心]：界面不阻塞与防连击保护
        // ==========================================
        // 1. 立即禁用按钮，防止用户狂点造成重复提交
        ui->confirmButton->setEnabled(false);

        // 2. 改变按钮文字和颜色，给用户明确反馈
        QString originalText = ui->confirmButton->text();
        ui->confirmButton->setText("⏳ 正在提交...");
        ui->confirmButton->setStyleSheet("background-color: #9E9E9E; color: white;");

        // 3. 启动一个 3 秒的安全超时定时器 (如果没连网，3秒后自动解冻弹窗)
        QTimer::singleShot(3000, this, [=]() {
            if (!ui->confirmButton->isEnabled()) {
                ui->confirmButton->setEnabled(true);
                ui->confirmButton->setText(originalText);
                ui->confirmButton->setStyleSheet(""); // 恢复原来样式
                QMessageBox::warning(this, tr("网络超时"), tr("服务器无响应，请重试！"));
            }
        });
        // ==========================================

    emit stationSubmitted(name, address, longitude, latitude, price);
}
