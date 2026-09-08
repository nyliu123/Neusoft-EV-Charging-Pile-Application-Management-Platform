#include "stationdetaildialog.h"
#include <QVBoxLayout>
#include <QHeaderView>

StationDetailDialog::StationDetailDialog(const QString &stationName, QWidget *parent)
    : QDialog(parent)
{
    // 动态显示当前选中的站点名称
    setWindowTitle("站内设备详情 - " + stationName);
    resize(700, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_deviceTable = new QTableView(this);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setAlternatingRowColors(true);

    // 按 NO.48 需求设置表头
    m_deviceModel = new QStandardItemModel(0, 6, this);
    m_deviceModel->setHeaderData(0, Qt::Horizontal, "设备编号");
    m_deviceModel->setHeaderData(1, Qt::Horizontal, "类型");
    m_deviceModel->setHeaderData(2, Qt::Horizontal, "功率(kW)");
    m_deviceModel->setHeaderData(3, Qt::Horizontal, "当前状态");
    m_deviceModel->setHeaderData(4, Qt::Horizontal, "累计结算次数");
    m_deviceModel->setHeaderData(5, Qt::Horizontal, "累计充电时长");

    m_deviceTable->setModel(m_deviceModel);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addWidget(m_deviceTable);

    // ======== 模拟数据（中期用）========
    QList<QStandardItem*> row1;
    row1 << new QStandardItem("DEV-001") << new QStandardItem("直流快充")
         << new QStandardItem("120") << new QStandardItem("空闲")
         << new QStandardItem("152") << new QStandardItem("320小时");
         
    QList<QStandardItem*> row2;
    row2 << new QStandardItem("DEV-002") << new QStandardItem("交流慢充")
         << new QStandardItem("7") << new QStandardItem("充电中")
         << new QStandardItem("89") << new QStandardItem("415小时");

    QList<QStandardItem*> row3;
    row3 << new QStandardItem("DEV-003") << new QStandardItem("直流快充")
         << new QStandardItem("120") << new QStandardItem("离线/故障")
         << new QStandardItem("201") << new QStandardItem("508小时");

    m_deviceModel->appendRow(row1);
    m_deviceModel->appendRow(row2);
    m_deviceModel->appendRow(row3);
}
