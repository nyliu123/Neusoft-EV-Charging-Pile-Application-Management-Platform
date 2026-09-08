#include "admin_pile_page.h"
#include "client_ui/animated_combo_box.h"

#include "admin_api_client.h"
#include "admin_format.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ev {

namespace {

constexpr int kColumnPileNumber = 0;
constexpr int kColumnStation = 1;
constexpr int kColumnType = 2;
constexpr int kColumnPower = 3;
constexpr int kColumnStatus = 4;
constexpr int kColumnCount = 5;
constexpr int kColumnDuration = 6;
constexpr int kColumnAction = 7;

} // namespace

AdminPilePage::AdminPilePage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // Header + filters.
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    auto *titleLabel = new QLabel(QStringLiteral("充电桩管理"), this);
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto *stationLabel = new QLabel(QStringLiteral("站点："), this);
    stationLabel->setProperty("uiClass", "formLabel");
    headerLayout->addWidget(stationLabel);
    stationBox_ = new ev::AnimatedComboBox(this);
    stationBox_->setMinimumWidth(180);
    headerLayout->addWidget(stationBox_);

    auto *filterStatusLabel = new QLabel(QStringLiteral("状态："), this);
    filterStatusLabel->setProperty("uiClass", "formLabel");
    headerLayout->addWidget(filterStatusLabel);
    statusBox_ = new ev::AnimatedComboBox(this);
    statusBox_->addItem(QStringLiteral("全部状态"), QString());
    statusBox_->addItem(QStringLiteral("空闲"), QStringLiteral("idle"));
    statusBox_->addItem(QStringLiteral("已预约"), QStringLiteral("reserved"));
    statusBox_->addItem(QStringLiteral("使用中"), QStringLiteral("in_use"));
    statusBox_->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    headerLayout->addWidget(statusBox_);

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setProperty("uiClass", "primary");
    headerLayout->addWidget(refreshButton_);
    rootLayout->addLayout(headerLayout);

    // Table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("桩编号"),
        QStringLiteral("所属站点"),
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
    loadingOverlay_->setProperty("uiClass", "loadingOverlay");
    loadingOverlay_->hide();

    connect(refreshButton_, &QPushButton::clicked, this, &AdminPilePage::reloadAll);
    connect(stationBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { loadPiles(); });
    connect(statusBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { loadPiles(); });
}

void AdminPilePage::reloadAll()
{
    loadStations();
}

void AdminPilePage::reload()
{
    loadPiles();
}

void AdminPilePage::loadStations()
{
    setLoading(true, QStringLiteral("正在加载站点列表..."));
    api_->sendStationOptions(this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            if (!ok) {
                setLoading(false, QString());
                setStatusText(QStringLiteral("站点列表加载失败：%1").arg(message), true);
                loadPiles();
                return;
            }
            const QJsonArray stations = result.value(QStringLiteral("stations")).toArray();
            const QSignalBlocker blocker(stationBox_);
            stationBox_->clear();
            stationBox_->addItem(QStringLiteral("全部站点"), 0);
            for (const QJsonValue &value : stations) {
                const QJsonObject station = value.toObject();
                stationBox_->addItem(
                    QStringLiteral("%1（%2 台桩）")
                        .arg(station.value(QStringLiteral("station_name")).toString())
                        .arg(station.value(QStringLiteral("pile_count")).toInt()),
                    station.value(QStringLiteral("station_id")).toInt());
            }
            loadPiles();
        });
}

void AdminPilePage::loadPiles()
{
    QJsonObject params;
    const int stationId = stationBox_->currentData().toInt();
    if (stationId > 0) {
        params.insert(QStringLiteral("station_id"), stationId);
    }
    const QString status = statusBox_->currentData().toString();
    if (!status.isEmpty()) {
        params.insert(QStringLiteral("status"), status);
    }

    setLoading(true, QStringLiteral("正在加载充电桩列表..."));
    api_->sendQuery(QStringLiteral("pile_list"), params, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            setLoading(false, QString());
            if (!ok) {
                table_->setRowCount(0);
                setStatusText(QStringLiteral("充电桩列表加载失败：%1").arg(message), true);
                return;
            }
            fillTable(result.value(QStringLiteral("piles")).toArray());
        });
}

void AdminPilePage::fillTable(const QJsonArray &piles)
{
    table_->setRowCount(0);

    if (piles.isEmpty()) {
        setStatusText(QStringLiteral("没有符合条件的充电桩"), false);
        return;
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(piles.size());
    int faultCount = 0;

    for (int row = 0; row < piles.size(); ++row) {
        const QJsonObject pile = piles[row].toObject();
        const QString status = pile.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("fault")) ++faultCount;

        // 桩编号
        auto *numItem = new QTableWidgetItem(
            pile.value(QStringLiteral("pile_number")).toString());
        numItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPileNumber, numItem);

        // 所属站点
        table_->setItem(row, kColumnStation,
            new QTableWidgetItem(pile.value(QStringLiteral("station_name")).toString()));

        // 类型
        auto *typeItem = new QTableWidgetItem(pileTypeText(
            pile.value(QStringLiteral("pile_type")).toString()));
        typeItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnType, typeItem);

        // 功率
        auto *powerItem = new QTableWidgetItem(
            QStringLiteral("%1 kW")
                .arg(pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 0));
        powerItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPower, powerItem);

        // 状态 (colored)
        auto *statusItem = new QTableWidgetItem(pileStatusText(status));
        statusItem->setForeground(pileStatusColor(status));
        statusItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnStatus, statusItem);

        // 累计充电次数
        auto *countItem = new QTableWidgetItem(
            QString::number(pile.value(QStringLiteral("total_charge_count")).toInt()));
        countItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnCount, countItem);

        // 累计时长
        auto *durItem = new QTableWidgetItem(
            QString::number(
                pile.value(QStringLiteral("total_charge_duration")).toDouble(), 'f', 1));
        durItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnDuration, durItem);

        // 操作：仅故障桩显示"远程重启"按钮，其他显示"—"
        if (status == QStringLiteral("fault")) {
            const long long pileId =
                pile.value(QStringLiteral("pile_id")).toInteger();
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
            placeholder->setProperty("uiClass", "muted");
            table_->setCellWidget(row, kColumnAction, placeholder);
        }
    }
    table_->setUpdatesEnabled(true);

    // Status bar: show fault count if any.
    if (faultCount > 0) {
        setStatusText(QStringLiteral("共 %1 台充电桩　·　故障 %2 台（可远程重启）")
                          .arg(piles.size()).arg(faultCount), false);
    } else {
        setStatusText(QStringLiteral("共 %1 台充电桩　·　全部正常")
                          .arg(piles.size()), false);
    }
}

void AdminPilePage::requestRestart(long long pileId, const QString &pileNumber, int row)
{
    const auto answer = QMessageBox::warning(this, QStringLiteral("远程重启确认"),
        QStringLiteral("确认远程重启充电桩 %1？\n\n"
                       "重启后该桩将恢复为空闲状态，用户端立即可见。").arg(pileNumber),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    // Disable the button on this row while the request is in flight.
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
                    QStringLiteral("充电桩 %1 重启成功，已恢复为空闲状态。")
                        .arg(pileNumber));
            } else {
                QMessageBox::warning(this, QStringLiteral("远程重启失败"), message);
            }
            // Always refresh so status reflects current truth.
            loadPiles();
        });
}

void AdminPilePage::setLoading(bool loading, const QString &message)
{
    if (loading) {
        loadingOverlay_->setText(message.isEmpty()
            ? QStringLiteral("加载中...") : message);
        loadingOverlay_->setGeometry(table_->rect());
        loadingOverlay_->raise();
        loadingOverlay_->show();
        table_->setEnabled(false);
        refreshButton_->setEnabled(false);
        stationBox_->setEnabled(false);
        statusBox_->setEnabled(false);
    } else {
        loadingOverlay_->hide();
        table_->setEnabled(true);
        refreshButton_->setEnabled(true);
        stationBox_->setEnabled(true);
        statusBox_->setEnabled(true);
    }
}

void AdminPilePage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("tone", isError ? "error" : "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

} // namespace ev
