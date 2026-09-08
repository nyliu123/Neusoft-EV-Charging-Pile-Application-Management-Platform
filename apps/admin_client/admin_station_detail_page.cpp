#include "admin_station_detail_page.h"

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

constexpr int kColumnPileNumber = 0;
constexpr int kColumnType = 1;
constexpr int kColumnPower = 2;
constexpr int kColumnStatus = 3;
constexpr int kColumnCount = 4;
constexpr int kColumnDuration = 5;
constexpr int kColumnAction = 6;

} // namespace

AdminStationDetailPage::AdminStationDetailPage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // Header: back button + title.
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    backButton_ = new QPushButton(QStringLiteral("← 返回列表"), this);
    backButton_->setProperty("uiClass", "text");
    backButton_->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(backButton_);
    auto *titleLabel = new QLabel(QStringLiteral("站内设备详情"), this);
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    rootLayout->addLayout(headerLayout);

    // Station info card.
    infoLabel_ = new QLabel(this);
    infoLabel_->setWordWrap(true);
    infoLabel_->setStyleSheet(QStringLiteral(
        "QLabel { background: #f5f7fa; color: #37474f; border-radius: 6px;"
        "  padding: 12px 14px; font-size: 13px; }"));
    rootLayout->addWidget(infoLabel_);

    // Pile table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("桩编号"),
        QStringLiteral("类型"),
        QStringLiteral("功率"),
        QStringLiteral("状态"),
        QStringLiteral("累计充电次数"),
        QStringLiteral("累计时长（小时）"),
        QStringLiteral("操作")
    });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnType, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPower, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnStatus, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnAction, QHeaderView::ResizeToContents);
    rootLayout->addWidget(table_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setProperty("tone", "muted");
    rootLayout->addWidget(statusLabel_);

    // Loading overlay — centred in the table area.
    loadingOverlay_ = new QLabel(table_);
    loadingOverlay_->setAlignment(Qt::AlignCenter);
    loadingOverlay_->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(255,255,255,200); color: #409eff;"
        "  font-size: 14px; border-radius: 8px; }"));
    loadingOverlay_->hide();

    connect(backButton_, &QPushButton::clicked, this,
            &AdminStationDetailPage::backRequested);
}

void AdminStationDetailPage::loadStation(long long stationId)
{
    stationId_ = stationId;
    reload();
}

void AdminStationDetailPage::reload()
{
    if (stationId_ <= 0) {
        return;
    }

    setLoading(true, QStringLiteral("正在加载站点详情..."));
    api_->sendQuery(QStringLiteral("station_detail"),
        QJsonObject {{QStringLiteral("station_id"), stationId_}}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            setLoading(false, QString());
            if (!ok) {
                table_->setRowCount(0);
                infoLabel_->setText(QString());
                setStatusText(QStringLiteral("站点详情加载失败：%1").arg(message), true);
                return;
            }
            applyStation(result.value(QStringLiteral("station")).toObject(),
                         result.value(QStringLiteral("piles")).toArray());
        });
}

void AdminStationDetailPage::applyStation(const QJsonObject &station,
                                          const QJsonArray &piles)
{
    infoLabel_->setText(QStringLiteral(
        "站名：%1　　地址：%2\n"
        "经纬度：%3, %4　　充电单价：¥%5/度　　充电桩总数：%6 台")
        .arg(station.value(QStringLiteral("station_name")).toString(),
             station.value(QStringLiteral("address")).toString())
        .arg(station.value(QStringLiteral("longitude")).toDouble(), 0, 'f', 6)
        .arg(station.value(QStringLiteral("latitude")).toDouble(), 0, 'f', 6)
        .arg(formatAmount(station.value(QStringLiteral("price_per_kwh")).toDouble()))
        .arg(piles.size()));
    fillTable(piles);
}

void AdminStationDetailPage::fillTable(const QJsonArray &piles)
{
    table_->setRowCount(0);

    if (piles.isEmpty()) {
        setStatusText(QStringLiteral("该站点暂无充电桩"), false);
        return;
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(piles.size());
    int faultCount = 0;

    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles[row].toObject();
        const QString status = pile.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("fault")) ++faultCount;

        auto *numItem = new QTableWidgetItem(
            pile.value(QStringLiteral("pile_number")).toString());
        numItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPileNumber, numItem);

        auto *typeItem = new QTableWidgetItem(pileTypeText(
            pile.value(QStringLiteral("pile_type")).toString()));
        typeItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnType, typeItem);

        auto *powerItem = new QTableWidgetItem(QStringLiteral("%1 kW")
            .arg(pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 0));
        powerItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPower, powerItem);

        auto *statusItem = new QTableWidgetItem(pileStatusText(status));
        statusItem->setForeground(pileStatusColor(status));
        statusItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnStatus, statusItem);

        auto *countItem = new QTableWidgetItem(QString::number(
            pile.value(QStringLiteral("total_charge_count")).toInt()));
        countItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnCount, countItem);

        auto *durItem = new QTableWidgetItem(QString::number(
            pile.value(QStringLiteral("total_charge_duration")).toDouble(), 'f', 1));
        durItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnDuration, durItem);

        if (status == QStringLiteral("fault")) {
            const long long pileId = pile.value(QStringLiteral("pile_id")).toInteger();
            const QString pileNumber = pile.value(QStringLiteral("pile_number")).toString();
            auto *restartButton = new QPushButton(QStringLiteral("远程重启"), table_);
            restartButton->setProperty("uiClass", "danger");
            restartButton->setCursor(Qt::PointingHandCursor);
            connect(restartButton, &QPushButton::clicked, this,
                [this, pileId, pileNumber, row] {
                    requestRestart(pileId, pileNumber, row);
                });
            table_->setCellWidget(row, kColumnAction, restartButton);
        } else {
            auto *placeholder = new QLabel(QStringLiteral("—"), table_);
            placeholder->setAlignment(Qt::AlignCenter);
            placeholder->setStyleSheet(QStringLiteral("QLabel { color: #c0c4cc; }"));
            table_->setCellWidget(row, kColumnAction, placeholder);
        }
    }
    table_->setUpdatesEnabled(true);

    setStatusText(faultCount > 0
        ? QStringLiteral("共 %1 台充电桩　·　故障 %2 台（可远程重启）")
              .arg(piles.size()).arg(faultCount)
        : QStringLiteral("共 %1 台充电桩　·　全部正常").arg(piles.size()), false);
}

void AdminStationDetailPage::requestRestart(long long pileId,
                                            const QString &pileNumber, int row)
{
    const auto answer = QMessageBox::warning(this, QStringLiteral("远程重启确认"),
        QStringLiteral("确认远程重启充电桩 %1？\n\n"
                       "重启后该桩将恢复为空闲状态，用户端立即可见。").arg(pileNumber),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    auto *btn = qobject_cast<QPushButton *>(table_->cellWidget(row, kColumnAction));
    if (btn) {
        btn->setEnabled(false);
        btn->setText(QStringLiteral("重启中..."));
    }

    api_->sendAction(QStringLiteral("restart_pile"),
        QJsonObject {{QStringLiteral("pile_id"), pileId}}, this,
        [this, pileNumber](bool ok, const QJsonObject &, const QString &message) {
            if (ok) {
                QMessageBox::information(this, QStringLiteral("远程重启成功"),
                    QStringLiteral("充电桩 %1 重启成功，已恢复为空闲状态。").arg(pileNumber));
            } else {
                QMessageBox::warning(this, QStringLiteral("远程重启失败"), message);
            }
            reload();
        });
}

void AdminStationDetailPage::setLoading(bool loading, const QString &message)
{
    if (loading) {
        loadingOverlay_->setText(message.isEmpty()
            ? QStringLiteral("加载中...") : message);
        loadingOverlay_->setGeometry(table_->rect());
        loadingOverlay_->raise();
        loadingOverlay_->show();
        table_->setEnabled(false);
        backButton_->setEnabled(false);
    } else {
        loadingOverlay_->hide();
        table_->setEnabled(true);
        backButton_->setEnabled(true);
    }
}

void AdminStationDetailPage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("tone", isError ? "error" : "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

} // namespace ev
