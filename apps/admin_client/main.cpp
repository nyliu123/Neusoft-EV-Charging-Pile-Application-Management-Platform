#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableView>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QHeaderView>
#include "addstationdialog.h"
#include "stationdetaildialog.h" // 引入设备详情弹窗
#include "network/platform_client.h"
#include "screendataservice.h"
#include <QJsonDocument>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>

class AdminMainWindow : public QMainWindow {
public:
    AdminMainWindow(ev::PlatformClient *client, QWidget *parent = nullptr)
        : QMainWindow(parent), m_client(client), m_addDialog(nullptr)
    {
        setWindowTitle("充电站后台管理系统");
        resize(900, 600);

        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        // 1. 顶部操作区
        QHBoxLayout *topLayout = new QHBoxLayout();
        QPushButton *btnAddStation = new QPushButton("新增充电站", this);
        btnAddStation->setMinimumHeight(35);
        QPushButton *btnRefresh = new QPushButton("刷新列表", this);
        btnRefresh->setMinimumHeight(35);

        topLayout->addWidget(btnAddStation);
        topLayout->addWidget(btnRefresh);
        topLayout->addStretch();

        // 2. 站点列表展示区
        QTableView *stationTable = new QTableView(this);
        stationTable->setAlternatingRowColors(true);
        stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

        m_stationModel = new QStandardItemModel(0, 5, this);
        m_stationModel->setHeaderData(0, Qt::Horizontal, "站点名称");
        m_stationModel->setHeaderData(1, Qt::Horizontal, "地址");
        m_stationModel->setHeaderData(2, Qt::Horizontal, "单价(元/度)");
        m_stationModel->setHeaderData(3, Qt::Horizontal, "设备总数");
        m_stationModel->setHeaderData(4, Qt::Horizontal, "空闲数");

        stationTable->setModel(m_stationModel);
        stationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        mainLayout->addLayout(topLayout);
        mainLayout->addWidget(stationTable);
        setCentralWidget(centralWidget);

        // 3. 连接 UI 信号
        connect(btnAddStation, &QPushButton::clicked, this, &AdminMainWindow::onAddStationClicked);
        connect(btnRefresh, &QPushButton::clicked, this, &AdminMainWindow::onRefreshClicked);
        connect(stationTable, &QTableView::doubleClicked, this, &AdminMainWindow::onStationDoubleClicked);

        // 4. 连接网络层信号
        connect(m_client, &ev::PlatformClient::addStationResult, this, &AdminMainWindow::handleAddStationResult);
    }

private:
    void onAddStationClicked() {
        if (!m_addDialog) {
            m_addDialog = new AddStationDialog(this);
            connect(m_addDialog, &AddStationDialog::stationSubmitted,
                    m_client, &ev::PlatformClient::sendAddStationRequest);
        }
        m_addDialog->show();
        m_addDialog->activateWindow();
    }

    void onRefreshClicked() {
        m_stationModel->removeRows(0, m_stationModel->rowCount());
        QList<QStandardItem*> row;
        row << new QStandardItem("东大一区充电站")
            << new QStandardItem("辽宁省沈阳市浑南区创新路195号")
            << new QStandardItem("1.20")
            << new QStandardItem("10")
            << new QStandardItem("3");
        m_stationModel->appendRow(row);
    }

    void onStationDoubleClicked(const QModelIndex &index) {
        if (!m_stationModel) return;
        QString stationName = m_stationModel->item(index.row(), 0)->text();
        StationDetailDialog dialog(stationName, this);
        dialog.exec();
    }

    void handleAddStationResult(bool success, const QString &message) {
        if (!m_addDialog) return;
        if (success) {
            QMessageBox::information(this, "操作成功", "新增充电站成功！\n" + message);
            m_addDialog->accept();
            onRefreshClicked();
        } else {
            QMessageBox::warning(this, "操作失败", "新增失败：" + message);
        }
    }

    ev::PlatformClient *m_client;
    AddStationDialog *m_addDialog;
    QStandardItemModel *m_stationModel;
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ev::PlatformClient client("AdminClient");
    AdminMainWindow w(&client);
    w.show();

    // ... main() 函数前面你原本的代码 (比如 QApplication a(argc, argv); 等) ...

    // --- 大屏数据测试弹窗开始 ---
    ScreenDataService service;
    QJsonObject bigScreenData = service.generateScreenData();
    QJsonDocument doc(bigScreenData);
    QString jsonString = doc.toJson(QJsonDocument::Indented);

    QDialog previewDialog;
    previewDialog.setWindowTitle("大屏数据与预警总控面板 (模拟)");
    previewDialog.resize(600, 700);

    QVBoxLayout* layout = new QVBoxLayout(&previewDialog);
    QTextEdit* textEdit = new QTextEdit(&previewDialog);
    textEdit->setReadOnly(true);
    // 依然保留极客风配色
    textEdit->setStyleSheet("background-color: #1e1e1e; color: #5ce6cd; font-family: Consolas; font-size: 14px;");
    textEdit->setText(jsonString);

    QPushButton* closeBtn = new QPushButton("确认并下发大屏", &previewDialog);
    QObject::connect(closeBtn, &QPushButton::clicked, &previewDialog, &QDialog::accept);

    layout->addWidget(textEdit);
    layout->addWidget(closeBtn);

    previewDialog.show(); // 显示弹窗
    // --- 大屏数据测试弹窗结束 ---

    return a.exec();

    return a.exec();
}
