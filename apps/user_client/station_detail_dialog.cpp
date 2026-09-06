#include "station_detail_dialog.h"

#include "user_api_client.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <utility>

namespace {

QString pileTypeText(const QString &type)
{
    return type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充");
}

QString pileStatusText(const QString &status)
{
    if (status == QStringLiteral("idle")) return QStringLiteral("空闲");
    if (status == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (status == QStringLiteral("in_use")) return QStringLiteral("使用中");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    return QStringLiteral("状态未知");
}

QColor pileStatusColor(const QString &status)
{
    if (status == QStringLiteral("idle")) return QColor(0x15, 0x73, 0x47);
    if (status == QStringLiteral("reserved")) return QColor(0xb2, 0x6a, 0x00);
    if (status == QStringLiteral("in_use")) return QColor(0x15, 0x65, 0xc0);
    return QColor(0xb4, 0x23, 0x18);
}

} // namespace

StationDetailDialog::StationDetailDialog(ev::UserApiClient *api, QJsonObject station,
                                         QWidget *parent)
    : QDialog(parent), api_(api), station_(std::move(station))
{
    setWindowTitle(QStringLiteral("充电站详情"));
    resize(760, 480);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(station_.value(QStringLiteral("station_name")).toString(), this);
    QFont font = title->font();
    font.setBold(true);
    font.setPointSize(16);
    title->setFont(font);
    layout->addWidget(title);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({QStringLiteral("桩编号"), QStringLiteral("类型"),
        QStringLiteral("功率"), QStringLiteral("状态"), QStringLiteral("操作")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table_, 1);

    statusLabel_ = new QLabel(QStringLiteral("正在加载站内设备..."), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #757575;"));
    layout->addWidget(statusLabel_);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    load();
}

void StationDetailDialog::load()
{
    const qint64 stationId = station_.value(QStringLiteral("station_id")).toInteger();
    api_->queryStationDetail(stationId, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            if (!ok) {
                table_->setRowCount(0);
                statusLabel_->setText(QStringLiteral("详情加载失败：%1").arg(message));
                statusLabel_->setStyleSheet(QStringLiteral("color: #b42318;"));
                return;
            }
            applyResult(result);
        });
}

void StationDetailDialog::applyResult(const QJsonObject &result)
{
    station_ = result.value(QStringLiteral("station")).toObject();
    const QJsonObject stats = result.value(QStringLiteral("stats")).toObject();
    summaryLabel_->setText(QStringLiteral("%1\n¥%2/度 · 在线率 %3% · 空闲 %4 / 总共 %5")
        .arg(station_.value(QStringLiteral("address")).toString())
        .arg(station_.value(QStringLiteral("price_per_kwh")).toDouble(), 0, 'f', 2)
        .arg(stats.value(QStringLiteral("online_rate")).toDouble() * 100.0, 0, 'f', 1)
        .arg(stats.value(QStringLiteral("idle")).toInt())
        .arg(stats.value(QStringLiteral("total")).toInt()));

    const QJsonArray piles = result.value(QStringLiteral("piles")).toArray();
    table_->setRowCount(piles.size());
    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles.at(row).toObject();
        const QString status = pile.value(QStringLiteral("status")).toString();
        table_->setItem(row, 0, new QTableWidgetItem(
            pile.value(QStringLiteral("pile_number")).toString()));
        table_->setItem(row, 1, new QTableWidgetItem(
            pileTypeText(pile.value(QStringLiteral("pile_type")).toString())));
        table_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 kW")
            .arg(pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 1)));
        auto *statusItem = new QTableWidgetItem(pileStatusText(status));
        statusItem->setForeground(pileStatusColor(status));
        table_->setItem(row, 3, statusItem);
        if (status == QStringLiteral("idle")) {
            auto *selectButton = new QPushButton(QStringLiteral("选择"), table_);
            connect(selectButton, &QPushButton::clicked, this, [this, pile] {
                emit pileSelected(station_, pile);
                accept();
            });
            table_->setCellWidget(row, 4, selectButton);
        } else {
            table_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("不可用")));
        }
    }
    statusLabel_->setText(piles.isEmpty()
        ? QStringLiteral("该站暂无充电桩")
        : QStringLiteral("只有标记为空闲的充电桩可进入后续预约流程"));
    statusLabel_->setStyleSheet(QStringLiteral("color: #757575;"));
}
