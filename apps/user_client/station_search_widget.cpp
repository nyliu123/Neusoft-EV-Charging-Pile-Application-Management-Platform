#include "station_search_widget.h"

#include "navigation_map_dialog.h"
#include "user_api_client.h"
#include "client_ui/animated_combo_box.h"
#include "client_ui/apple_widgets.h"

#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QString pileTypeText(const QString &type)
{
    return type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充");
}

QString distanceText(double distanceKm)
{
    if (distanceKm < 0.1) {
        return QStringLiteral("< 100 米");
    }
    if (distanceKm < 1.0) {
        return QStringLiteral("%1 米").arg(qRound(distanceKm * 1000.0));
    }
    QString value = QString::number(distanceKm, 'f', 1);
    if (value.endsWith(QStringLiteral(".0"))) {
        value.chop(2);
    }
    return QStringLiteral("%1 公里").arg(value);
}

} // namespace

StationSearchWidget::StationSearchWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget(this);
    root->addWidget(pages_);

    listPage_ = new QWidget(pages_);
    auto *listPageLayout = new QVBoxLayout(listPage_);
    listPageLayout->setContentsMargins(0, 0, 0, 0);
    listPageLayout->setSpacing(12);
    heroBanner_ = new ev::ModernHeroBanner(listPage_);
    listPageLayout->addWidget(heroBanner_);
    auto *searchCard = new QFrame(listPage_);
    searchCard->setObjectName(QStringLiteral("discoverySearch"));
    auto *searchCardLayout = new QVBoxLayout(searchCard);
    searchCardLayout->setContentsMargins(14, 12, 14, 12);
    searchCardLayout->setSpacing(8);
    auto *locationModeRow = new QHBoxLayout;
    locationModeRow->setSpacing(16);
    auto *locationModeLabel = new QLabel(QStringLiteral("定位方式："), searchCard);
    locationModeLabel->setProperty("uiClass", "muted");
    listLocationButton_ = new QRadioButton(QStringLiteral("从列表中选择"), searchCard);
    manualLocationButton_ = new QRadioButton(QStringLiteral("手动输入"), searchCard);
    listLocationButton_->setChecked(true);
    locationModeRow->addWidget(locationModeLabel);
    locationModeRow->addWidget(listLocationButton_);
    locationModeRow->addWidget(manualLocationButton_);
    locationModeRow->addStretch();
    searchCardLayout->addLayout(locationModeRow);
    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(12);
    areaBox_ = new ev::AnimatedComboBox(searchCard);
    areaBox_->addItem(QStringLiteral("（无）"), QString());
    const QStringList stationProvinces {
        QStringLiteral("辽宁省"), QStringLiteral("北京市"),
        QStringLiteral("河北省"), QStringLiteral("山西省"),
        QStringLiteral("吉林省"), QStringLiteral("黑龙江省"),
        QStringLiteral("江苏省"), QStringLiteral("浙江省"),
        QStringLiteral("安徽省"), QStringLiteral("福建省"),
        QStringLiteral("江西省"), QStringLiteral("山东省"),
        QStringLiteral("河南省"), QStringLiteral("湖北省"),
        QStringLiteral("湖南省"), QStringLiteral("广东省"),
        QStringLiteral("海南省"), QStringLiteral("四川省"),
        QStringLiteral("贵州省"), QStringLiteral("云南省"),
        QStringLiteral("陕西省"), QStringLiteral("甘肃省"),
        QStringLiteral("青海省"), QStringLiteral("台湾省")
    };
    for (const QString &province : stationProvinces) {
        areaBox_->addItem(province, province);
    }
    areaBox_->setMinimumWidth(180);
    areaBox_->setMaximumWidth(240);
    addressEdit_ = new QLineEdit(searchCard);
    addressEdit_->setPlaceholderText(QStringLiteral("请输入当前位置，例如：大连市软件园路8号"));
    addressEdit_->setClearButtonEnabled(true);
    addressEdit_->hide();
    searchButton_ = new QPushButton(QStringLiteral("搜索"), searchCard);
    searchButton_->setProperty("uiClass", "primary");
    searchRow->addWidget(areaBox_);
    searchRow->addWidget(addressEdit_, 1);
    searchRow->addWidget(searchButton_);
    auto *searchRowSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding,
                                             QSizePolicy::Minimum);
    searchRow->addItem(searchRowSpacer);
    searchButton_->hide();
    searchCardLayout->addLayout(searchRow);
    listPageLayout->addWidget(searchCard);
    auto *resultHeading = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("探索附近"), listPage_);
    title->setProperty("uiClass", "sectionTitle");
    resultHeading->addWidget(title);
    distanceBox_ = new ev::AnimatedComboBox(listPage_);
    distanceBox_->addItem(QStringLiteral("不限"), -1.0);
    distanceBox_->addItem(QStringLiteral("1 公里"), 1.0);
    distanceBox_->addItem(QStringLiteral("3 公里"), 3.0);
    distanceBox_->addItem(QStringLiteral("5 公里"), 5.0);
    distanceBox_->addItem(QStringLiteral("10 公里"), 10.0);
    distanceBox_->addItem(QStringLiteral("20 公里"), 20.0);
    distanceBox_->setCurrentIndex(0);
    distanceBox_->setMinimumWidth(100);
    distanceBox_->hide();
    resultHeading->addWidget(distanceBox_);
    resultHeading->addStretch();

    statusLabel_ = new QLabel(listPage_);
    statusLabel_->setProperty("uiClass", "muted");
    resultHeading->addWidget(statusLabel_);
    listPageLayout->addLayout(resultHeading);

    auto *scroll = new QScrollArea(listPage_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    stationGrid_ = new ev::AdaptiveStationGrid(scroll);
    scroll->setWidget(stationGrid_);
    stationResultsPage_ = scroll;
    resultPages_ = new QStackedWidget(listPage_);
    resultPages_->addWidget(stationResultsPage_);
    resultMessageLabel_ = new QLabel(resultPages_);
    resultMessageLabel_->setAlignment(Qt::AlignCenter);
    resultMessageLabel_->setProperty("uiClass", "sectionTitle");
    resultMessageLabel_->setText(QStringLiteral("你似乎来到了没有充电站的荒漠～"));
    resultPages_->addWidget(resultMessageLabel_);
    resultPages_->setCurrentWidget(resultMessageLabel_);
    listPageLayout->addWidget(resultPages_, 1);
    auto *attribution = new QLabel(QStringLiteral("输入位置或选择区域搜索 · 位置数据 © OpenStreetMap contributors"), listPage_);
    attribution->setProperty("uiClass", "muted");
    attribution->setWordWrap(true);
    listPageLayout->addWidget(attribution);
    pages_->addWidget(listPage_);

    detailPage_ = new QWidget(pages_);
    auto *detailLayout = new QVBoxLayout(detailPage_);
    detailTitle_ = new QLabel(detailPage_);
    detailTitle_->setProperty("uiClass", "pageTitle");
    detailLayout->addWidget(detailTitle_);
    detailMeta_ = new QLabel(detailPage_);
    detailMeta_->setProperty("uiClass", "stationMeta");
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
    commentsButton_ = new QPushButton(QStringLiteral("查看评论"), detailPage_);
    commentsButton_->setProperty("uiClass", "secondary");
    detailFooter->addWidget(commentsButton_);
    startNavigationButton_ = new QPushButton(QStringLiteral("打开地图导航"), detailPage_);
    startNavigationButton_->setProperty("uiClass", "primary");
    detailFooter->addWidget(startNavigationButton_);
    detailLayout->addLayout(detailFooter);
    pages_->addWidget(detailPage_);

    commentsPage_ = new QWidget(pages_);
    auto *commentsLayout = new QVBoxLayout(commentsPage_);
    commentsTitle_ = new QLabel(commentsPage_);
    commentsTitle_->setProperty("uiClass", "pageTitle");
    commentsLayout->addWidget(commentsTitle_);
    commentsSummary_ = new QLabel(commentsPage_);
    commentsSummary_->setProperty("uiClass", "muted");
    commentsSummary_->setWordWrap(true);
    commentsLayout->addWidget(commentsSummary_);

    auto *commentsScroll = new QScrollArea(commentsPage_);
    commentsScroll->setWidgetResizable(true);
    commentsScroll->setFrameShape(QFrame::NoFrame);
    auto *commentsList = new QWidget(commentsScroll);
    commentsListLayout_ = new QVBoxLayout(commentsList);
    commentsListLayout_->setContentsMargins(0, 4, 8, 4);
    commentsListLayout_->setSpacing(10);
    commentsListLayout_->addStretch();
    commentsScroll->setWidget(commentsList);
    commentsLayout->addWidget(commentsScroll, 1);

    auto *composeCard = new QFrame(commentsPage_);
    composeCard->setProperty("uiClass", "card");
    auto *composeLayout = new QVBoxLayout(composeCard);
    composeLayout->setContentsMargins(12, 10, 12, 10);
    auto *starRow = new QHBoxLayout;
    starRow->addWidget(new QLabel(QStringLiteral("打星："), composeCard));
    for (int i = 1; i <= 5; ++i) {
        auto *star = new QPushButton(QStringLiteral("☆"), composeCard);
        star->setProperty("uiClass", "ratingStar");
        star->setAccessibleName(QStringLiteral("%1 星").arg(i));
        star->setObjectName(QStringLiteral("ratingStar%1").arg(i));
        star->setFixedSize(36, 36);
        starRow->addWidget(star);
        connect(star, &QPushButton::clicked, this, [this, i] {
            setStarRating(starRating_ == i ? 0 : i);
        });
        starButtons_.append(star);
    }
    starValueLabel_ = new QLabel(QStringLiteral("未打星（0 分）"), composeCard);
    starValueLabel_->setProperty("uiClass", "muted");
    starRow->addWidget(starValueLabel_);
    starRow->addStretch();
    composeLayout->addLayout(starRow);
    commentInput_ = new QPlainTextEdit(composeCard);
    commentInput_->setAccessibleName(QStringLiteral("站点评价内容"));
    commentInput_->setPlaceholderText(QStringLiteral("说说这次充电的体验（1~200 字）"));
    commentInput_->setMaximumHeight(76);
    composeLayout->addWidget(commentInput_);
    auto *composeButtons = new QHBoxLayout;
    composeButtons->addStretch();
    publishCommentButton_ = new QPushButton(QStringLiteral("发表评论"), composeCard);
    publishCommentButton_->setProperty("uiClass", "primary");
    composeButtons->addWidget(publishCommentButton_);
    composeLayout->addLayout(composeButtons);
    commentsLayout->addWidget(composeCard);

    auto *commentsFooter = new QHBoxLayout;
    auto *backFromComments = new QPushButton(
        QStringLiteral("< 返回站点详情"), commentsPage_);
    backFromComments->setProperty("uiClass", "text");
    commentsFooter->addWidget(backFromComments);
    commentsFooter->addStretch();
    commentsLayout->addLayout(commentsFooter);
    pages_->addWidget(commentsPage_);

    connect(searchButton_, &QPushButton::clicked, this, &StationSearchWidget::search);
    connect(addressEdit_, &QLineEdit::returnPressed, this, &StationSearchWidget::search);
    connect(listLocationButton_, &QRadioButton::toggled, this,
            [this, searchRow, searchRowSpacer](bool checked) {
        areaBox_->setVisible(checked);
        addressEdit_->setVisible(!checked);
        searchButton_->setVisible(!checked);
        distanceBox_->setVisible(!checked || areaBox_->currentIndex() > 0);
        searchRowSpacer->changeSize(0, 0,
            checked ? QSizePolicy::Expanding : QSizePolicy::Fixed,
            QSizePolicy::Minimum);
        searchRow->invalidate();
        if (checked) {
            areaBox_->setFocus();
        } else {
            addressEdit_->setFocus();
        }
    });
    connect(areaBox_, &QComboBox::activated, this, [this](int index) {
        distanceBox_->setVisible(index > 0);
        if (index > 0) {
            addressEdit_->clear();
            search();
        } else {
            hasOriginLocation_ = false;
            originAddress_.clear();
            stationGrid_->clear();
            statusLabel_->clear();
            resultMessageLabel_->setText(QStringLiteral("你似乎来到了没有充电站的荒漠～"));
            resultPages_->setCurrentWidget(resultMessageLabel_);
        }
    });
    connect(distanceBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] {
        if (hasOriginLocation_) {
            loadStations(true, originLongitude_, originLatitude_);
        }
    });
    connect(backButton, &QPushButton::clicked, this, [this] {
        pages_->setCurrentWidget(listPage_);
    });
    connect(startNavigationButton_, &QPushButton::clicked,
            this, &StationSearchWidget::startNavigation);
    connect(commentsButton_, &QPushButton::clicked, this, [this] {
        if (detailStationId_ > 0) {
            showComments(detailStationId_);
        }
    });
    connect(publishCommentButton_, &QPushButton::clicked,
            this, &StationSearchWidget::publishComment);
    connect(backFromComments, &QPushButton::clicked, this, [this] {
        pages_->setCurrentWidget(detailPage_);
    });
}

void StationSearchWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Short windows prioritize complete station cards over decorative content.
    heroBanner_->setCompact(height() < 590);
}

void StationSearchWidget::refresh()
{
    pages_->setCurrentWidget(listPage_);
    listLocationButton_->setChecked(true);
    areaBox_->setCurrentIndex(0);
    distanceBox_->setCurrentIndex(0);
    distanceBox_->hide();
    addressEdit_->clear();
    hasOriginLocation_ = false;
    originAddress_.clear();
    stationGrid_->clear();
    statusLabel_->clear();
    resultMessageLabel_->setText(QStringLiteral("你似乎来到了没有充电站的荒漠～"));
    resultPages_->setCurrentWidget(resultMessageLabel_);
}

void StationSearchWidget::search()
{
    const bool useList = listLocationButton_->isChecked();
    const QString address = useList ? areaBox_->currentData().toString()
                                    : addressEdit_->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 useList ? QStringLiteral("请从列表中选择区域")
                                         : QStringLiteral("请输入当前位置"));
        return;
    }
    originAddress_ = address;
    setBusy(true, QStringLiteral("正在通过 OpenStreetMap 解析位置…"));
    api_->geocode(address, this,
                  [this](bool success, const QJsonObject &result, const QString &message) {
        if (!success) {
            setBusy(false);
            QMessageBox::warning(this, QStringLiteral("定位失败"),
                message.isEmpty() ? QStringLiteral("地图服务暂不可用，请稍后重试") : message);
            resultMessageLabel_->setText(QStringLiteral("定位失败，请稍后重试"));
            resultPages_->setCurrentWidget(resultMessageLabel_);
            return;
        }
        loadStations(true, result.value(QStringLiteral("longitude")).toDouble(),
                     result.value(QStringLiteral("latitude")).toDouble());
    });
}

void StationSearchWidget::loadStations(bool hasLocation, double longitude, double latitude)
{
    hasOriginLocation_ = hasLocation;
    originLongitude_ = longitude;
    originLatitude_ = latitude;
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
            resultMessageLabel_->setText(QStringLiteral("充电站读取失败"));
            resultPages_->setCurrentWidget(resultMessageLabel_);
            return;
        }
        renderStations(result, hasLocation);
    });
}

void StationSearchWidget::renderStations(const QJsonObject &result, bool locationAvailable)
{
    stationGrid_->clear();
    const QJsonArray allStations = result.value(QStringLiteral("stations")).toArray();
    QJsonArray stations;
    const double radiusKm = distanceBox_->currentData().toDouble();
    for (const QJsonValue &value : allStations) {
        const QJsonValue distance = value.toObject().value(QStringLiteral("distance_km"));
        if (distance.isDouble() && (radiusKm < 0.0 || distance.toDouble() <= radiusKm)) {
            stations.append(value);
        }
    }
    statusLabel_->setProperty("tone", "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
    if (stations.isEmpty()) {
        statusLabel_->clear();
        resultMessageLabel_->setText(QStringLiteral("你似乎来到了没有充电站的荒漠～"));
        resultPages_->setCurrentWidget(resultMessageLabel_);
        return;
    }
    resultPages_->setCurrentWidget(stationResultsPage_);
    statusLabel_->setText(locationAvailable
        ? (radiusKm < 0.0
              ? QStringLiteral("共找到 %1 个模拟站点").arg(stations.size())
              : QStringLiteral("%1 公里内共找到 %2 个模拟站点")
                    .arg(radiusKm, 0, 'g', 3).arg(stations.size()))
        : QStringLiteral("尚未定位或地图位置不可用，当前按站点编号展示 %1 个模拟站点")
              .arg(stations.size()));
    for (const QJsonValue &value : stations) {
        const QJsonObject station = value.toObject();
        const int idle = station.value(QStringLiteral("idle_count")).toInt();
        const QJsonValue distance = station.value(QStringLiteral("distance_km"));
        const QString distanceText = distance.isDouble()
            ? ::distanceText(distance.toDouble())
            : QStringLiteral("距离未知");
        const QString availability = idle > 0
            ? QStringLiteral("空闲 %1 / 总共 %2").arg(idle).arg(
                  station.value(QStringLiteral("total_piles")).toInt())
            : QStringLiteral("暂无空闲 · 总共 %1").arg(
                  station.value(QStringLiteral("total_piles")).toInt());
        double ratingValue = -1.0;
        QString hotReview;
        const QJsonObject rating = station.value(QStringLiteral("rating")).toObject();
        if (rating.value(QStringLiteral("count")).toInt() > 0) {
            ratingValue = rating.value(QStringLiteral("avg")).toDouble();
            const QJsonObject hot = rating.value(QStringLiteral("hot")).toObject();
            if (!hot.isEmpty()) {
                hotReview = QStringLiteral("“%1”").arg(
                    hot.value(QStringLiteral("content")).toString());
            }
        }
        auto *card = new ev::AppleStationCard(
            station.value(QStringLiteral("station_name")).toString(),
            station.value(QStringLiteral("address")).toString(), distanceText,
            QStringLiteral("¥%1/度").arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2),
            availability, ratingValue, hotReview, idle > 0, listPage_);
        connect(card, &QPushButton::clicked, this, [this, station] {
            showStationDetail(station);
        });
        stationGrid_->addCard(card);
    }
}

void StationSearchWidget::showStationDetail(const QJsonObject &station)
{
    pages_->setCurrentWidget(detailPage_);
    detailStationId_ = station.value(QStringLiteral("station_id")).toInteger();
    destinationName_ = station.value(QStringLiteral("station_name")).toString();
    destinationAddress_ = station.value(QStringLiteral("address")).toString();
    hasDestinationLocation_ = station.value(QStringLiteral("longitude")).isDouble()
        && station.value(QStringLiteral("latitude")).isDouble();
    destinationLongitude_ = station.value(QStringLiteral("longitude")).toDouble();
    destinationLatitude_ = station.value(QStringLiteral("latitude")).toDouble();
    detailTitle_->setText(destinationName_);
    detailMeta_->setText(QStringLiteral("正在读取站内充电桩…"));
    updateNavigationAvailability();
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
    destinationName_ = station.value(QStringLiteral("station_name")).toString();
    destinationAddress_ = station.value(QStringLiteral("address")).toString();
    if (station.value(QStringLiteral("longitude")).isDouble()
        && station.value(QStringLiteral("latitude")).isDouble()) {
        hasDestinationLocation_ = true;
        destinationLongitude_ = station.value(QStringLiteral("longitude")).toDouble();
        destinationLatitude_ = station.value(QStringLiteral("latitude")).toDouble();
    }
    detailTitle_->setText(station.value(QStringLiteral("station_name")).toString());
    const QJsonObject rating = station.value(QStringLiteral("rating")).toObject();
    QString ratingText;
    if (rating.value(QStringLiteral("count")).toInt() > 0) {
        ratingText = QStringLiteral("  ·  ★ %1 %2（%3 条评论）")
            .arg(rating.value(QStringLiteral("avg")).toDouble(), 0, 'f', 1)
            .arg(rating.value(QStringLiteral("tier")).toString())
            .arg(rating.value(QStringLiteral("count")).toInt());
    } else {
        ratingText = QStringLiteral("  ·  暂无评分");
    }
    detailMeta_->setText(QStringLiteral("%1  ·  ¥%2/度%3")
        .arg(station.value(QStringLiteral("address")).toString())
        .arg(station.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2)
        .arg(ratingText));
    commentsButton_->setText(
        rating.value(QStringLiteral("count")).toInt() > 0
            ? QStringLiteral("查看评论（%1）").arg(rating.value(QStringLiteral("count")).toInt())
            : QStringLiteral("查看评论"));

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
            emit pileChosen(pile.value(QStringLiteral("pile_id")).toInteger());
        });
        pileTable_->setCellWidget(row, 3, buttonCell);
    }
    updateNavigationAvailability();
}

void StationSearchWidget::updateNavigationAvailability()
{
    const bool canNavigate = hasOriginLocation_ && hasDestinationLocation_;
    startNavigationButton_->setEnabled(canNavigate);
}

void StationSearchWidget::startNavigation()
{
    if (!hasOriginLocation_ || !hasDestinationLocation_) {
        return;
    }
    auto *map = new NavigationMapDialog(originLongitude_, originLatitude_,
        destinationLongitude_, destinationLatitude_, originAddress_, destinationName_,
        destinationAddress_, 0, this);
    connect(api_, &ev::UserApiClient::sessionExpired, map, &QDialog::close);
    map->show();
}

void StationSearchWidget::setBusy(bool busy, const QString &message)
{
    areaBox_->setEnabled(!busy);
    addressEdit_->setEnabled(!busy);
    searchButton_->setEnabled(!busy);
    distanceBox_->setEnabled(!busy);
    searchButton_->setText(QStringLiteral("搜索"));
    if (busy) {
        stationGrid_->clear();
        resultMessageLabel_->setText(QStringLiteral("正在玩命加载中..."));
        resultPages_->setCurrentWidget(resultMessageLabel_);
    }
    if (!message.isEmpty()) {
        statusLabel_->setText(message);
    }
}

void StationSearchWidget::showComments(qint64 stationId)
{
    commentsStationId_ = stationId;
    commentsTitle_->setText(destinationName_);
    commentsSummary_->setText(QStringLiteral("正在读取评论…"));
    pages_->setCurrentWidget(commentsPage_);
    loadComments();
}

void StationSearchWidget::loadComments()
{
    api_->listComments(commentsStationId_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
        if (!success) {
            commentsSummary_->setText(
                message.isEmpty() ? QStringLiteral("评论读取失败") : message);
            return;
        }
        renderComments(result);
    });
}

void StationSearchWidget::renderComments(const QJsonObject &result)
{
    while (commentsListLayout_->count() > 1) {
        QLayoutItem *item = commentsListLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    const QJsonObject summary = result.value(QStringLiteral("summary")).toObject();
    if (summary.value(QStringLiteral("count")).toInt() > 0) {
        commentsSummary_->setText(QStringLiteral("平均 %1 分 · %2 · 共 %3 条评论")
            .arg(summary.value(QStringLiteral("avg")).toDouble(), 0, 'f', 1)
            .arg(summary.value(QStringLiteral("tier")).toString())
            .arg(summary.value(QStringLiteral("count")).toInt()));
    } else {
        commentsSummary_->setText(QStringLiteral("暂无评论，来抢沙发吧"));
    }

    // Preserve the draft while re-rendering (e.g. after a like toggle).
    const QString draftText = commentInput_->toPlainText();
    const int draftStars = starRating_;
    bool mineLoaded = false;
    const QJsonArray comments = result.value(QStringLiteral("comments")).toArray();
    int index = 0;
    for (const QJsonValue &value : comments) {
        const QJsonObject comment = value.toObject();
        const bool isMine = comment.value(QStringLiteral("is_mine")).toBool();
        if (isMine && !mineLoaded) {
            mineLoaded = true;
            commentInput_->setPlainText(comment.value(QStringLiteral("content")).toString());
            setStarRating(comment.value(QStringLiteral("rating")).toInt());
            publishCommentButton_->setText(QStringLiteral("修改我的评论"));
        }
        const int rating = comment.value(QStringLiteral("rating")).toInt();
        QString stars;
        for (int i = 1; i <= 5; ++i) {
            stars += i <= rating ? QStringLiteral("★") : QStringLiteral("☆");
        }
        auto *card = new QFrame(commentsPage_);
        card->setProperty("uiClass", "card");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        auto *headerRow=new QHBoxLayout;
        auto *author=new QLabel(comment.value(QStringLiteral("nickname")).toString()+QStringLiteral("  ·"),card);author->setProperty("uiClass","muted");headerRow->addWidget(author);
        auto *starLabel=new QLabel(stars,card);starLabel->setProperty("uiClass","commentStars");headerRow->addWidget(starLabel);
        auto *meta=new QLabel(QStringLiteral("·  %1%2").arg(comment.value(QStringLiteral("created_at")).toString(),isMine?QStringLiteral("  ·  我的评论"):QString()),card);meta->setProperty("uiClass","muted");headerRow->addWidget(meta);headerRow->addStretch();cardLayout->addLayout(headerRow);
        auto *content = new QLabel(comment.value(QStringLiteral("content")).toString(), card);
        content->setWordWrap(true);
        cardLayout->addWidget(content);
        auto *footerRow = new QHBoxLayout;
        footerRow->addStretch();
        const bool liked = comment.value(QStringLiteral("liked_by_me")).toBool();
        auto *likeButton = new QPushButton(QStringLiteral("%1 %2")
            .arg(liked ? QStringLiteral("已赞") : QStringLiteral("赞"))
            .arg(comment.value(QStringLiteral("like_count")).toInt()), card);
        likeButton->setProperty("uiClass", liked ? "primary" : "secondary");
        const qint64 commentId = comment.value(QStringLiteral("comment_id")).toInteger();
        connect(likeButton, &QPushButton::clicked, this, [this, commentId] {
            onToggleLike(commentId);
        });
        footerRow->addWidget(likeButton);
        cardLayout->addLayout(footerRow);
        commentsListLayout_->insertWidget(index++, card);
    }
    if (!mineLoaded) {
        commentInput_->setPlainText(draftText);
        setStarRating(draftStars);
        publishCommentButton_->setText(QStringLiteral("发表评论"));
    }
}

void StationSearchWidget::publishComment()
{
    const QString content = commentInput_->toPlainText().trimmed();
    if (content.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写评论内容"));
        return;
    }
    publishCommentButton_->setEnabled(false);
    api_->postComment(commentsStationId_, content, starRating_, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
        publishCommentButton_->setEnabled(true);
        if (!success) {
            QMessageBox::warning(this, QStringLiteral("发表失败"), message);
            return;
        }
        QMessageBox::information(this, QStringLiteral("成功"),
            result.value(QStringLiteral("updated")).toBool()
                ? QStringLiteral("评论已更新")
                : QStringLiteral("评论已发表"));
        loadComments();
    });
}

void StationSearchWidget::onToggleLike(qint64 commentId)
{
    api_->toggleCommentLike(commentId, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
        if (!success) {
            QMessageBox::warning(this, QStringLiteral("操作失败"), message);
            return;
        }
        loadComments();
    });
}

void StationSearchWidget::setStarRating(int rating)
{
    starRating_ = rating;
    for (int i = 0; i < starButtons_.size(); ++i) {
        starButtons_.at(i)->setText(
            i < rating ? QStringLiteral("★") : QStringLiteral("☆"));
    }
    starValueLabel_->setText(rating > 0
        ? QStringLiteral("%1 星（%2 分）").arg(rating).arg(rating * 2)
        : QStringLiteral("未打星（0 分）"));
}
