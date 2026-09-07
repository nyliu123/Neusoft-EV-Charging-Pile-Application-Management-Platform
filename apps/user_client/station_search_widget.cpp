#include "station_search_widget.h"

#include "user_api_client.h"
#include "client_ui/animated_combo_box.h"

#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString pileTypeText(const QString &type)
{
    return type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充");
}

} // namespace

StationSearchWidget::StationSearchWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    pages_ = new QStackedWidget(this);
    root->addWidget(pages_);

    listPage_ = new QWidget(pages_);
    auto *listPageLayout = new QVBoxLayout(listPage_);
    auto *title = new QLabel(QStringLiteral("附近充电站"), listPage_);
    title->setProperty("uiClass", "pageTitle");
    listPageLayout->addWidget(title);
    auto *description = new QLabel(
        QStringLiteral("选择预设区域或输入地址。距离为直线距离，仅供找桩参考。位置数据 © OpenStreetMap contributors。"),
        listPage_);
    description->setProperty("uiClass", "muted");
    listPageLayout->addWidget(description);

    auto *searchRow = new QHBoxLayout;
    areaBox_ = new ev::AnimatedComboBox(listPage_);
    areaBox_->addItem(QStringLiteral("选择预设区域"), QString());
    areaBox_->addItem(QStringLiteral("大连市甘井子区"), QStringLiteral("辽宁省大连市甘井子区"));
    areaBox_->addItem(QStringLiteral("大连市高新区"),
                      QStringLiteral("辽宁省大连市高新区"));
    areaBox_->addItem(QStringLiteral("大连北站"), QStringLiteral("辽宁省大连市大连北站"));
    addressEdit_ = new QLineEdit(listPage_);
    addressEdit_->setPlaceholderText(QStringLiteral("或手动输入地址，例如：大连市软件园路8号"));
    addressEdit_->setClearButtonEnabled(true);
    searchButton_ = new QPushButton(QStringLiteral("搜索"), listPage_);
    searchButton_->setProperty("uiClass", "primary");
    searchRow->addWidget(areaBox_);
    searchRow->addWidget(addressEdit_, 1);
    searchRow->addWidget(searchButton_);
    listPageLayout->addLayout(searchRow);

    statusLabel_ = new QLabel(listPage_);
    statusLabel_->setProperty("uiClass", "muted");
    listPageLayout->addWidget(statusLabel_);

    auto *scroll = new QScrollArea(listPage_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *stationList = new QWidget(scroll);
    stationList->setObjectName(QStringLiteral("stationList"));
    stationListLayout_ = new QVBoxLayout(stationList);
    stationListLayout_->setContentsMargins(0, 4, 8, 4);
    stationListLayout_->setSpacing(12);
    stationListLayout_->addStretch();
    scroll->setWidget(stationList);
    listPageLayout->addWidget(scroll, 1);
    pages_->addWidget(listPage_);

    detailPage_ = new QWidget(pages_);
    auto *detailLayout = new QVBoxLayout(detailPage_);
    detailTitle_ = new QLabel(detailPage_);
    detailTitle_->setProperty("uiClass", "pageTitle");
    detailLayout->addWidget(detailTitle_);
    detailMeta_ = new QLabel(detailPage_);
    detailMeta_->setProperty("uiClass", "muted");
    detailMeta_->setWordWrap(true);
    detailLayout->addWidget(detailMeta_);
    pileTable_ = new QTableWidget(detailPage_);
    pileTable_->setColumnCount(4);
    pileTable_->setHorizontalHeaderLabels({QStringLiteral("桩编号"), QStringLiteral("类型"),
                                           QStringLiteral("功率"), QStringLiteral("操作")});
    pileTable_->setAlternatingRowColors(true);
    pileTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pileTable_->setSelectionMode(QAbstractItemView::NoSelection);
    pileTable_->verticalHeader()->hide();
    pileTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    pileTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    pileTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    pileTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    pileTable_->setColumnWidth(3, 64);
    pileTable_->verticalHeader()->setDefaultSectionSize(34);
    detailLayout->addWidget(pileTable_, 1);
    auto *detailFooter = new QHBoxLayout;
    auto *backButton = new QPushButton(QStringLiteral("< 返回站点列表"), detailPage_);
    backButton->setProperty("uiClass", "text");
    detailFooter->addWidget(backButton);
    detailFooter->addStretch();
    detailLayout->addLayout(detailFooter);
    pages_->addWidget(detailPage_);

    connect(searchButton_, &QPushButton::clicked, this, &StationSearchWidget::search);
    connect(addressEdit_, &QLineEdit::returnPressed, this, &StationSearchWidget::search);
    connect(areaBox_, &QComboBox::activated, this, [this](int index) {
        if (index > 0) {
            addressEdit_->clear();
            search();
        }
    });
    connect(backButton, &QPushButton::clicked, this, [this] {
        pages_->setCurrentWidget(listPage_);
    });
}

void StationSearchWidget::refresh()
{
    pages_->setCurrentWidget(listPage_);
    areaBox_->setCurrentIndex(0);
    addressEdit_->clear();
    while (stationListLayout_->count() > 1) {
        QLayoutItem *item = stationListLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    statusLabel_->setText(QStringLiteral("请选择区域或输入地址后搜索"));
}

void StationSearchWidget::search()
{
    const QString manualAddress = addressEdit_->text().trimmed();
    const QString address = !manualAddress.isEmpty()
        ? manualAddress : areaBox_->currentData().toString();
    if (address.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请选择区域或输入地址"));
        return;
    }
    setBusy(true, QStringLiteral("正在通过 OpenStreetMap 解析位置…"));
    api_->geocode(address, this,
                  [this](bool success, const QJsonObject &result, const QString &message) {
        if (!success) {
            QMessageBox::warning(this, QStringLiteral("定位失败"),
                message.isEmpty() ? QStringLiteral("地图服务暂不可用，请稍后重试") : message);
            loadStations();
            return;
        }
        loadStations(true, result.value(QStringLiteral("longitude")).toDouble(),
                     result.value(QStringLiteral("latitude")).toDouble());
    });
}

void StationSearchWidget::loadStations(bool hasLocation, double longitude, double latitude)
{
    setBusy(true, hasLocation ? QStringLiteral("正在按距离查询充电站…")
                              : QStringLiteral("正在读取充电站列表…"));
    api_->queryStations(hasLocation, longitude, latitude, this,
                        [this, hasLocation](bool success, const QJsonObject &result,
                                            const QString &message) {
        setBusy(false);
        if (!success) {
            statusLabel_->setText(message.isEmpty() ? QStringLiteral("充电站读取失败") : message);
            statusLabel_->setProperty("tone", "error");
            statusLabel_->style()->unpolish(statusLabel_);
            statusLabel_->style()->polish(statusLabel_);
            return;
        }
        renderStations(result, hasLocation);
    });
}

void StationSearchWidget::renderStations(const QJsonObject &result, bool locationAvailable)
{
    while (stationListLayout_->count() > 1) {
        QLayoutItem *item = stationListLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    const QJsonArray stations = result.value(QStringLiteral("stations")).toArray();
    statusLabel_->setProperty("tone", "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
    if (stations.isEmpty()) {
        statusLabel_->setText(QStringLiteral("附近暂无充电站"));
        return;
    }
    statusLabel_->setText(locationAvailable
        ? QStringLiteral("共找到 %1 个模拟站点，已按直线距离排序").arg(stations.size())
        : QStringLiteral("尚未定位或地图位置不可用，当前按站点编号展示 %1 个模拟站点")
              .arg(stations.size()));
    int index = 0;
    for (const QJsonValue &value : stations) {
        const QJsonObject station = value.toObject();
        const int idle = station.value(QStringLiteral("idle_count")).toInt();
        const QJsonValue distance = station.value(QStringLiteral("distance_km"));
        const QString distanceText = distance.isDouble()
            ? QStringLiteral("%1 km").arg(distance.toDouble(), 0, 'f', 2)
            : QStringLiteral("距离未知");
        const QString availability = idle > 0
            ? QStringLiteral("空闲 %1 / 总共 %2").arg(idle).arg(
                  station.value(QStringLiteral("total_piles")).toInt())
            : QStringLiteral("暂无空闲 · 总共 %1").arg(
                  station.value(QStringLiteral("total_piles")).toInt());
        auto *card = new QPushButton(
            QStringLiteral("%1    ·    %2\n%3\n¥%4/度    %5")
                .arg(station.value(QStringLiteral("station_name")).toString(), distanceText,
                     station.value(QStringLiteral("address")).toString())
                .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2)
                .arg(availability),
            listPage_);
        card->setProperty("uiClass", "stationCard");
        card->setProperty("available", idle > 0);
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(96);
        connect(card, &QPushButton::clicked, this, [this, station] {
            showStationDetail(station);
        });
        stationListLayout_->insertWidget(index++, card);
    }
}

void StationSearchWidget::showStationDetail(const QJsonObject &station)
{
    pages_->setCurrentWidget(detailPage_);
    detailTitle_->setText(station.value(QStringLiteral("station_name")).toString());
    detailMeta_->setText(QStringLiteral("正在读取站内充电桩…"));
    pileTable_->setRowCount(0);
    api_->queryPiles(station.value(QStringLiteral("station_id")).toInteger(), this,
                     [this](bool success, const QJsonObject &result, const QString &message) {
        if (!success) {
            detailMeta_->setText(message.isEmpty() ? QStringLiteral("站点详情读取失败") : message);
            QMessageBox::warning(this, QStringLiteral("读取失败"), detailMeta_->text());
            return;
        }
        renderStationDetail(result);
    });
}

void StationSearchWidget::renderStationDetail(const QJsonObject &result)
{
    const QJsonObject station = result.value(QStringLiteral("station")).toObject();
    detailTitle_->setText(station.value(QStringLiteral("station_name")).toString());
    detailMeta_->setText(QStringLiteral("%1  ·  ¥%2/度")
        .arg(station.value(QStringLiteral("address")).toString())
        .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2));

    const QJsonArray piles = result.value(QStringLiteral("piles")).toArray();
    QJsonArray idlePiles;
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        if (pile.value(QStringLiteral("status")).toString() == QStringLiteral("idle")) {
            idlePiles.append(pile);
        }
    }
    pileTable_->setRowCount(idlePiles.size());
    for (int row = 0; row < idlePiles.size(); ++row) {
        const QJsonObject pile = idlePiles.at(row).toObject();
        pileTable_->setItem(row, 0, new QTableWidgetItem(
            pile.value(QStringLiteral("pile_number")).toString()));
        pileTable_->setItem(row, 1, new QTableWidgetItem(
            pileTypeText(pile.value(QStringLiteral("pile_type")).toString())));
        pileTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 kW").arg(
            pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 1)));
        auto *buttonCell = new QWidget(pileTable_);
        auto *buttonLayout = new QHBoxLayout(buttonCell);
        buttonLayout->setContentsMargins(4, 3, 4, 3);
        auto *choose = new QPushButton(QStringLiteral("选择"), buttonCell);
        choose->setProperty("uiClass", "compactPrimary");
        choose->setFixedSize(46, 24);
        buttonLayout->addWidget(choose, 0, Qt::AlignCenter);
        connect(choose, &QPushButton::clicked, this, [this, pile] {
            QMessageBox::information(this, QStringLiteral("已选择充电桩"),
                QStringLiteral("已选择 %1（%2 kW），可继续进入预约充电流程。")
                    .arg(pile.value(QStringLiteral("pile_number")).toString())
                    .arg(pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 1));
        });
        pileTable_->setCellWidget(row, 3, buttonCell);
    }
}

void StationSearchWidget::setBusy(bool busy, const QString &message)
{
    areaBox_->setEnabled(!busy);
    addressEdit_->setEnabled(!busy);
    searchButton_->setEnabled(!busy);
    searchButton_->setText(busy ? QStringLiteral("查询中…") : QStringLiteral("搜索"));
    if (!message.isEmpty()) {
        statusLabel_->setText(message);
    }
}
