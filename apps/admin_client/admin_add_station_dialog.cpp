#include "admin_add_station_dialog.h"

#include "admin_api_client.h"
#include "admin_format.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ev {

namespace {

bool isValidCoordinateText(const QString &text, double min, double max, double *out)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || value < min || value > max) {
        return false;
    }
    if (out) {
        *out = value;
    }
    return true;
}

} // namespace

AdminAddStationDialog::AdminAddStationDialog(AdminApiClient *api, QWidget *parent)
    : QDialog(parent), api_(api)
{
    setWindowTitle(QStringLiteral("新增充电站"));
    setMinimumWidth(480);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(14);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("1~100 字符"));
    nameEdit_->setMaxLength(100);
    formLayout->addRow(QStringLiteral("站名："), nameEdit_);

    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText(QStringLiteral("详细地址，用于解析经纬度"));
    addressEdit_->setMaxLength(255);
    formLayout->addRow(QStringLiteral("地址："), addressEdit_);

    // Coordinate row: read-only fields + geocode / manual-entry controls.
    auto *coordLayout = new QHBoxLayout();
    coordLayout->setSpacing(6);
    longitudeEdit_ = new QLineEdit(this);
    longitudeEdit_->setPlaceholderText(QStringLiteral("经度"));
    longitudeEdit_->setReadOnly(true);
    longitudeEdit_->setFixedWidth(110);
    coordLayout->addWidget(longitudeEdit_);
    latitudeEdit_ = new QLineEdit(this);
    latitudeEdit_->setPlaceholderText(QStringLiteral("纬度"));
    latitudeEdit_->setReadOnly(true);
    latitudeEdit_->setFixedWidth(110);
    coordLayout->addWidget(latitudeEdit_);
    geocodeButton_ = new QPushButton(QStringLiteral("获取经纬度"), this);
    geocodeButton_->setProperty("uiClass", "secondary");
    coordLayout->addWidget(geocodeButton_);
    manualButton_ = new QPushButton(QStringLiteral("手动填写"), this);
    manualButton_->setProperty("uiClass", "text");
    coordLayout->addWidget(manualButton_);
    coordLayout->addStretch();
    formLayout->addRow(QStringLiteral("经纬度："), coordLayout);

    priceSpin_ = new QDoubleSpinBox(this);
    priceSpin_->setRange(0.01, 999.99);
    priceSpin_->setDecimals(2);
    priceSpin_->setSingleStep(0.1);
    priceSpin_->setValue(1.20);
    priceSpin_->setSuffix(QStringLiteral(" 元/度"));
    formLayout->addRow(QStringLiteral("充电单价："), priceSpin_);
    rootLayout->addLayout(formLayout);

    hintLabel_ = new QLabel(
        QStringLiteral("填写地址后点击“获取经纬度”自动解析；也可点击“手动填写”直接输入。"), this);
    hintLabel_->setProperty("tone", "muted");
    hintLabel_->setWordWrap(true);
    rootLayout->addWidget(hintLabel_);

    // Buttons.
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setProperty("uiClass", "secondary");
    buttonLayout->addWidget(cancelButton);
    submitButton_ = new QPushButton(QStringLiteral("提交"), this);
    submitButton_->setProperty("uiClass", "primary");
    submitButton_->setDefault(true);
    buttonLayout->addWidget(submitButton_);
    rootLayout->addLayout(buttonLayout);

    connect(geocodeButton_, &QPushButton::clicked,
            this, &AdminAddStationDialog::fetchCoordinates);
    connect(manualButton_, &QPushButton::clicked, this, [this] {
        setManualCoordinatesEnabled(true);
        hintLabel_->setText(QStringLiteral("已解锁手动输入，请填写合法经纬度。"));
    });
    connect(submitButton_, &QPushButton::clicked,
            this, &AdminAddStationDialog::validateAndSubmit);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString AdminAddStationDialog::stationName() const
{
    return stationName_;
}

void AdminAddStationDialog::fetchCoordinates()
{
    const QString address = addressEdit_->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先填写地址，再获取经纬度。"));
        return;
    }

    geocodeButton_->setEnabled(false);
    geocodeButton_->setText(QStringLiteral("解析中..."));
    api_->sendGeocode(address, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            geocodeButton_->setEnabled(true);
            geocodeButton_->setText(QStringLiteral("获取经纬度"));
            if (!ok) {
                setManualCoordinatesEnabled(true);
                hintLabel_->setText(QStringLiteral(
                    "地址无法解析（%1），已解锁手动填写。").arg(message));
                return;
            }
            longitudeEdit_->setText(QString::number(
                result.value(QStringLiteral("longitude")).toDouble(), 'f', 6));
            latitudeEdit_->setText(QString::number(
                result.value(QStringLiteral("latitude")).toDouble(), 'f', 6));
            const QString source = result.value(QStringLiteral("source")).toString();
            const int confidence = result.value(QStringLiteral("confidence")).toInt();
            const QString display =
                result.value(QStringLiteral("display_address")).toString();
            hintLabel_->setText(QStringLiteral("解析成功：%1（来源：%2，置信度 %3）")
                                    .arg(display.isEmpty() ? QStringLiteral("—") : display,
                                         source.isEmpty() ? QStringLiteral("—") : source)
                                    .arg(confidence));
        });
}

void AdminAddStationDialog::setManualCoordinatesEnabled(bool enabled)
{
    longitudeEdit_->setReadOnly(!enabled);
    latitudeEdit_->setReadOnly(!enabled);
}

bool AdminAddStationDialog::validateAndSubmit()
{
    const QString name = nameEdit_->text().trimmed();
    const QString address = addressEdit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
            QStringLiteral("请输入充电站名称。"));
        nameEdit_->setFocus();
        return false;
    }
    if (address.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
            QStringLiteral("请输入充电站地址。"));
        addressEdit_->setFocus();
        return false;
    }

    double longitude = 0.0;
    double latitude = 0.0;
    if (!isValidCoordinateText(longitudeEdit_->text(), -180.0, 180.0, &longitude)) {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
            QStringLiteral("请获取或填写合法经度（-180 ~ 180）。"));
        return false;
    }
    if (!isValidCoordinateText(latitudeEdit_->text(), -90.0, 90.0, &latitude)) {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
            QStringLiteral("请获取或填写合法纬度（-90 ~ 90）。"));
        return false;
    }
    if (priceSpin_->value() <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
            QStringLiteral("充电单价必须大于 0。"));
        return false;
    }

    submitButton_->setEnabled(false);
    submitButton_->setText(QStringLiteral("提交中..."));
    api_->sendAction(QStringLiteral("add_station"),
        QJsonObject {
            {QStringLiteral("station_name"), name},
            {QStringLiteral("address"), address},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("latitude"), latitude},
            {QStringLiteral("price_per_kwh"), priceSpin_->value()}
        }, this,
        [this, name](bool ok, const QJsonObject &, const QString &message) {
            submitButton_->setEnabled(true);
            submitButton_->setText(QStringLiteral("提交"));
            if (!ok) {
                QMessageBox::warning(this, QStringLiteral("创建失败"), message);
                return;
            }
            stationName_ = name;
            accept();
        });
    return true;
}

} // namespace ev
