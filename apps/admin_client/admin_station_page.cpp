#include "admin_station_page.h"

#include "admin_add_station_dialog.h"
#include "admin_api_client.h"
#include "admin_format.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ev {

namespace {

constexpr int kColumnName = 0;
constexpr int kColumnAddress = 1;
constexpr int kColumnCoord = 2;
constexpr int kColumnPrice = 3;
constexpr int kColumnPileCount = 4;
constexpr int kColumnAction = 5;

} // namespace

AdminStationPage::AdminStationPage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // Header.
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    auto *titleLabel = new QLabel(QStringLiteral("充电站管理"), this);
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setProperty("uiClass", "secondary");
    headerLayout->addWidget(refreshButton_);
    addButton_ = new QPushButton(QStringLiteral("新增充电站"), this);
    addButton_->setProperty("uiClass", "primary");
    headerLayout->addWidget(addButton_);
    rootLayout->addLayout(headerLayout);

    // Table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("站名"),
        QStringLiteral("地址"),
        QStringLiteral("经纬度"),
        QStringLiteral("充电单价"),
        QStringLiteral("充电桩数量"),
        QStringLiteral("操作")
    });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnCoord, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPrice, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPileCount, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnAction, QHeaderView::ResizeToContents);
    rootLayout->addWidget(table_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setProperty("tone", "muted");
    rootLayout->addWidget(statusLabel_);

    // Loading overlay — centred in the table area.
    loadingOverlay_ = new QLabel(table_);
    loadingOverlay_->setAlignment(Qt::AlignCenter);
    loadingOverlay_->setProperty("uiClass", "loadingOverlay");
    loadingOverlay_->hide();

    connect(refreshButton_, &QPushButton::clicked, this, &AdminStationPage::loadStations);
    connect(addButton_, &QPushButton::clicked, this, &AdminStationPage::openAddDialog);
}

void AdminStationPage::reload()
{
    loadStations();
}

void AdminStationPage::loadStations()
{
    setLoading(true, QStringLiteral("正在加载充电站列表..."));
    api_->sendQuery(QStringLiteral("station_list"), QJsonObject {}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            setLoading(false, QString());
            if (!ok) {
                table_->setRowCount(0);
                setStatusText(QStringLiteral("充电站列表加载失败：%1").arg(message), true);
                return;
            }
            fillTable(result.value(QStringLiteral("stations")).toArray());
        });
}

void AdminStationPage::fillTable(const QJsonArray &stations)
{
    table_->setRowCount(0);

    if (stations.isEmpty()) {
        setStatusText(QStringLiteral("暂无充电站，点击右上角“新增充电站”创建"), false);
        return;
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(stations.size());

    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations[row].toObject();

        table_->setItem(row, kColumnName,
            new QTableWidgetItem(station.value(QStringLiteral("station_name")).toString()));

        table_->setItem(row, kColumnAddress,
            new QTableWidgetItem(station.value(QStringLiteral("address")).toString()));

        auto *coordItem = new QTableWidgetItem(QStringLiteral("%1, %2")
            .arg(station.value(QStringLiteral("longitude")).toDouble(), 0, 'f', 6)
            .arg(station.value(QStringLiteral("latitude")).toDouble(), 0, 'f', 6));
        coordItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnCoord, coordItem);

        auto *priceItem = new QTableWidgetItem(QStringLiteral("¥%1/度").arg(
            formatAmount(station.value(QStringLiteral("price_per_kwh")).toDouble())));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, kColumnPrice, priceItem);

        auto *countItem = new QTableWidgetItem(
            QString::number(station.value(QStringLiteral("pile_count")).toInt()));
        countItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPileCount, countItem);

        auto *detailButton = new QPushButton(QStringLiteral("查看详情"), table_);
        detailButton->setProperty("uiClass", "text");
        detailButton->setCursor(Qt::PointingHandCursor);
        connect(detailButton, &QPushButton::clicked, this, [this, station] {
            emit detailRequested(
                station.value(QStringLiteral("station_id")).toInteger());
        });
        table_->setCellWidget(row, kColumnAction, detailButton);
    }
    table_->setUpdatesEnabled(true);

    setStatusText(QStringLiteral("共 %1 个充电站").arg(stations.size()), false);
}

void AdminStationPage::openAddDialog()
{
    AdminAddStationDialog dialog(api_, this);
    if (dialog.exec() == QDialog::Accepted) {
        QMessageBox::information(this, QStringLiteral("创建成功"),
            QStringLiteral("充电站 %1 创建成功。").arg(dialog.stationName()));
        loadStations();
    }
}

void AdminStationPage::setLoading(bool loading, const QString &message)
{
    if (loading) {
        loadingOverlay_->setText(message.isEmpty()
            ? QStringLiteral("加载中...") : message);
        loadingOverlay_->setGeometry(table_->rect());
        loadingOverlay_->raise();
        loadingOverlay_->show();
        table_->setEnabled(false);
        refreshButton_->setEnabled(false);
        addButton_->setEnabled(false);
    } else {
        loadingOverlay_->hide();
        table_->setEnabled(true);
        refreshButton_->setEnabled(true);
        addButton_->setEnabled(true);
    }
}

void AdminStationPage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("tone", isError ? "error" : "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

} // namespace ev
