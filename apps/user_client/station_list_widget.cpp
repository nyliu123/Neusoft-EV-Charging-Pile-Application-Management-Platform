#include "station_list_widget.h"

#include "station_card.h"
#include "station_detail_dialog.h"
#include "user_api_client.h"

#include <QComboBox>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

StationListWidget::StationListWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("查找充电站"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *locationBar = new QHBoxLayout;
    presetBox_ = new QComboBox(this);
    presetBox_->addItem(QStringLiteral("选择预设区域"), QString());
    presetBox_->addItem(QStringLiteral("大连软件园"), QStringLiteral("大连市甘井子区软件园"));
    presetBox_->addItem(QStringLiteral("甘井子区"), QStringLiteral("大连市甘井子区"));
    presetBox_->addItem(QStringLiteral("高新园区"), QStringLiteral("大连市高新园区"));
    presetBox_->addItem(QStringLiteral("大连北站"), QStringLiteral("大连市甘井子区大连北站"));
    locationBar->addWidget(presetBox_);
    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText(QStringLiteral("或输入地址进行模拟定位"));
    locationBar->addWidget(addressEdit_, 1);
    searchButton_ = new QPushButton(QStringLiteral("搜索并按距离排序"), this);
    locationBar->addWidget(searchButton_);
    modeBox_ = new QComboBox(this);
    modeBox_->addItem(QStringLiteral("驾车"), QStringLiteral("driving"));
    modeBox_->addItem(QStringLiteral("步行"), QStringLiteral("walking"));
    locationBar->addWidget(modeBox_);
    root->addLayout(locationBar);

    statusLabel_ = new QLabel(QStringLiteral("正在加载站点列表..."), this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color: #616161;"));
    root->addWidget(statusLabel_);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *cards = new QWidget(scroll);
    cardsLayout_ = new QVBoxLayout(cards);
    cardsLayout_->setContentsMargins(0, 0, 4, 0);
    cardsLayout_->setSpacing(10);
    scroll->setWidget(cards);
    root->addWidget(scroll, 1);

    connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QString address = presetBox_->currentData().toString();
        if (!address.isEmpty()) {
            addressEdit_->setText(address);
        }
    });
    connect(searchButton_, &QPushButton::clicked, this,
            &StationListWidget::resolveLocation);
    connect(addressEdit_, &QLineEdit::returnPressed, this,
            &StationListWidget::resolveLocation);
}

void StationListWidget::refresh()
{
    pendingWarning_.clear();
    loadStations();
}

void StationListWidget::resolveLocation()
{
    const QString address = addressEdit_->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请选择区域或输入地址"));
        return;
    }
    searchButton_->setEnabled(false);
    statusLabel_->setText(QStringLiteral("正在解析位置..."));
    api_->geocode(address, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            if (!ok) {
                searchButton_->setEnabled(true);
                pendingWarning_ = message;
                loadStations();
                return;
            }
            loadStations(result.value(QStringLiteral("longitude")).toDouble(),
                         result.value(QStringLiteral("latitude")).toDouble(),
                         result.value(QStringLiteral("source")).toString());
        });
}

void StationListWidget::loadStations()
{
    hasOrigin_ = false;
    api_->queryStations(this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            searchButton_->setEnabled(true);
            if (!ok) {
                clearCards();
                statusLabel_->setText(QStringLiteral("站点加载失败：%1").arg(message));
                statusLabel_->setStyleSheet(QStringLiteral("color: #b42318;"));
                return;
            }
            showStations(result, {});
        });
}

void StationListWidget::loadStations(double longitude, double latitude,
                                     const QString &source)
{
    hasOrigin_ = true;
    originLongitude_ = longitude;
    originLatitude_ = latitude;
    api_->queryStations(longitude, latitude, this,
        [this, source](bool ok, const QJsonObject &result, const QString &message) {
            searchButton_->setEnabled(true);
            if (!ok) {
                clearCards();
                statusLabel_->setText(QStringLiteral("站点加载失败：%1").arg(message));
                statusLabel_->setStyleSheet(QStringLiteral("color: #b42318;"));
                return;
            }
            showStations(result, source);
        });
}

void StationListWidget::showStations(const QJsonObject &result, const QString &source)
{
    clearCards();
    const QJsonArray stations = result.value(QStringLiteral("stations")).toArray();
    if (stations.isEmpty()) {
        statusLabel_->setText(QStringLiteral("附近暂无充电站"));
        statusLabel_->setStyleSheet(QStringLiteral("color: #616161;"));
        return;
    }
    if (!pendingWarning_.isEmpty()) {
        statusLabel_->setText(QStringLiteral("%1；已加载 %2 个文字站点，距离暂不可用。")
            .arg(pendingWarning_).arg(stations.size()));
        statusLabel_->setStyleSheet(QStringLiteral("color: #b26a00;"));
        pendingWarning_.clear();
    } else {
        statusLabel_->setText(source.isEmpty()
            ? QStringLiteral("共 %1 个模拟站点；设置位置后可按直线距离排序。").arg(stations.size())
            : QStringLiteral("共 %1 个模拟站点，已按直线距离排序（坐标来源：%2；非道路里程）。")
                  .arg(stations.size()).arg(source));
        statusLabel_->setStyleSheet(QStringLiteral("color: #616161;"));
    }

    for (const QJsonValue &value : stations) {
        auto *card = new StationCard(value.toObject(), this);
        connect(card, &StationCard::stationSelected, this,
                [this](const QJsonObject &station) {
            auto *dialog = new StationDetailDialog(api_, station, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &StationDetailDialog::pileSelected,
                    this, &StationListWidget::pileSelected);
            dialog->open();
        });
        connect(card, &StationCard::navigationRequested,
                this, &StationListWidget::requestNavigation);
        cardsLayout_->addWidget(card);
    }
    cardsLayout_->addStretch();
}

void StationListWidget::requestNavigation(const QJsonObject &station)
{
    if (!hasOrigin_) {
        QMessageBox::information(this, QStringLiteral("导航"),
            QStringLiteral("请先输入位置并完成距离排序，再规划路线。"));
        return;
    }
    if (!station.value(QStringLiteral("longitude")).isDouble()
        || !station.value(QStringLiteral("latitude")).isDouble()) {
        QMessageBox::warning(this, QStringLiteral("导航"),
                             QStringLiteral("该站点缺少有效坐标。"));
        return;
    }
    emit navigationRequested(station);
    const QString mode = modeBox_->currentData().toString();
    statusLabel_->setStyleSheet(QString());
    statusLabel_->setText(QStringLiteral("正在规划%1路线...")
        .arg(mode == QStringLiteral("walking") ? QStringLiteral("步行")
                                                : QStringLiteral("驾车")));
    api_->route(originLongitude_, originLatitude_,
                station.value(QStringLiteral("longitude")).toDouble(),
                station.value(QStringLiteral("latitude")).toDouble(),
                mode, this,
        [this, station, mode](bool ok, const QJsonObject &result,
                              const QString &message) {
            if (!ok) {
                statusLabel_->setText(QStringLiteral("路线规划失败：%1").arg(message));
                statusLabel_->setStyleSheet(QStringLiteral("color: #b42318;"));
                QMessageBox::warning(this, QStringLiteral("导航失败"), message);
                return;
            }
            const QString modeText = mode == QStringLiteral("walking")
                ? QStringLiteral("步行") : QStringLiteral("驾车");
            const QString summary = QStringLiteral(
                "%1路线：当前位置 → %2\n预计距离 %3 km，约 %4 分钟。")
                .arg(modeText,
                     station.value(QStringLiteral("station_name")).toString())
                .arg(result.value(QStringLiteral("distance_km")).toDouble(), 0, 'f', 2)
                .arg(result.value(QStringLiteral("duration_minutes")).toInt());
            statusLabel_->setText(summary);
            statusLabel_->setStyleSheet(QStringLiteral("color: #157347;"));
            QMessageBox::information(this, QStringLiteral("路线规划结果"), summary);
        });
}

void StationListWidget::clearCards()
{
    while (QLayoutItem *item = cardsLayout_->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
