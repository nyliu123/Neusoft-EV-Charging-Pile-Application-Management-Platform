#include "admin_pile_page.h"

#include "admin_api_client.h"
#include "admin_format.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
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
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    headerLayout->addWidget(new QLabel(QStringLiteral("站点："), this));
    stationBox_ = new QComboBox(this);
    stationBox_->setMinimumWidth(180);
    headerLayout->addWidget(stationBox_);

    headerLayout->addWidget(new QLabel(QStringLiteral("状态："), this));
    statusBox_ = new QComboBox(this);
    statusBox_->addItem(QStringLiteral("全部状态"), QString());
    statusBox_->addItem(QStringLiteral("空闲"), QStringLiteral("idle"));
    statusBox_->addItem(QStringLiteral("已预约"), QStringLiteral("reserved"));
    statusBox_->addItem(QStringLiteral("使用中"), QStringLiteral("in_use"));
    statusBox_->addItem(QStringLiteral("故障"), QStringLiteral("fault"));
    headerLayout->addWidget(statusBox_);

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
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
    table_->setStyleSheet(QStringLiteral(
        "QTableWidget { border: 1px solid #e0e6ed; gridline-color: #eef1f4; }"
        "QHeaderView::section { background: #f5f7fa; border: none;"
        " border-bottom: 1px solid #e0e6ed; padding: 6px; }"));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnType, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPower, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnStatus, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnAction, QHeaderView::ResizeToContents);
    rootLayout->addWidget(table_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #757575; font-size: 12px;"));
    rootLayout->addWidget(statusLabel_);

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
    api_->sendQuery(QStringLiteral("station_list"), QJsonObject {}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            if (!ok) {
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

    api_->sendQuery(QStringLiteral("pile_list"), params, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
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
    int row = 0;
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        const QString status = pile.value(QStringLiteral("status")).toString();
        table_->insertRow(row);

        table_->setItem(row, kColumnPileNumber,
            new QTableWidgetItem(pile.value(QStringLiteral("pile_number")).toString()));
        table_->setItem(row, kColumnStation,
            new QTableWidgetItem(pile.value(QStringLiteral("station_name")).toString()));
        table_->setItem(row, kColumnType,
            new QTableWidgetItem(pileTypeText(pile.value(QStringLiteral("pile_type")).toString())));
        table_->setItem(row, kColumnPower, new QTableWidgetItem(
            QStringLiteral("%1 kW")
                .arg(pile.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 0)));

        auto *statusItem = new QTableWidgetItem(pileStatusText(status));
        statusItem->setForeground(pileStatusColor(status));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        table_->setItem(row, kColumnStatus, statusItem);

        table_->setItem(row, kColumnCount, new QTableWidgetItem(
            QString::number(pile.value(QStringLiteral("total_charge_count")).toInt())));
        table_->setItem(row, kColumnDuration, new QTableWidgetItem(
            QString::number(pile.value(QStringLiteral("total_charge_duration")).toDouble(), 'f', 1)));

        if (status == QStringLiteral("fault")) {
            const long long pileId =
                pile.value(QStringLiteral("pile_id")).toInteger();
            const QString pileNumber = pile.value(QStringLiteral("pile_number")).toString();
            auto *restartButton = new QPushButton(QStringLiteral("远程重启"), table_);
            restartButton->setCursor(Qt::PointingHandCursor);
            connect(restartButton, &QPushButton::clicked, this, [this, pileId, pileNumber] {
                requestRestart(pileId, pileNumber);
            });
            table_->setCellWidget(row, kColumnAction, restartButton);
        }
        ++row;
    }

    setStatusText(QStringLiteral("共 %1 台充电桩").arg(row), false);
}

void AdminPilePage::requestRestart(long long pileId, const QString &pileNumber)
{
    const auto answer = QMessageBox::question(this, QStringLiteral("远程重启"),
        QStringLiteral("确认远程重启充电桩 %1？重启后该桩将恢复为空闲状态。").arg(pileNumber),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    api_->sendAction(QStringLiteral("restart_pile"),
        QJsonObject {{QStringLiteral("pile_id"), pileId}}, this,
        [this, pileNumber](bool ok, const QJsonObject &, const QString &message) {
            QMessageBox::information(this, QStringLiteral("远程重启"),
                ok ? QStringLiteral("充电桩 %1 重启成功，已恢复为空闲状态。").arg(pileNumber)
                   : message);
            loadPiles();
        });
}

void AdminPilePage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(isError
        ? QStringLiteral("color: #c62828; font-size: 12px;")
        : QStringLiteral("color: #757575; font-size: 12px;"));
}

} // namespace ev
