#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QUrl>
#include "network/platform_client.h"

// 用户端主界面类 (完美终极版：美观 UI + 真实浏览器导航)
class UserMainWindow : public QMainWindow {
public:
    UserMainWindow(ev::PlatformClient *client, QWidget *parent = nullptr)
        : QMainWindow(parent), m_client(client), m_currentMode("car")
    {
        setWindowTitle("新能源充电 - 用户寻桩端");
        resize(400, 700);

        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(15, 15, 15, 15);

        // 1. 顶部：出行方式选择区
        QHBoxLayout *modeLayout = new QHBoxLayout();
        QLabel *modeLabel = new QLabel("出行方式：", this);
        QRadioButton *btnDrive = new QRadioButton("驾车", this);
        QRadioButton *btnWalk = new QRadioButton("步行", this);
        btnDrive->setChecked(true);

        QButtonGroup *modeGroup = new QButtonGroup(this);
        modeGroup->addButton(btnDrive);
        modeGroup->addButton(btnWalk);

        modeLayout->addWidget(modeLabel);
        modeLayout->addWidget(btnDrive);
        modeLayout->addWidget(btnWalk);
        modeLayout->addStretch();

        // 2. 中间：极美观的交互式占位面板 (不依赖网络，100% 成功展示)
        m_mapLabel = new QLabel(this);
        m_mapLabel->setAlignment(Qt::AlignCenter);
        m_mapLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // 3. 底部：距离、耗时与导航按钮
        QHBoxLayout *infoLayout = new QHBoxLayout();
        m_distanceLabel = new QLabel("距离：-- km", this);
        m_timeLabel = new QLabel("预计耗时：-- 分钟", this);

        QFont infoFont = m_distanceLabel->font();
        infoFont.setPointSize(11);
        infoFont.setBold(true);
        m_distanceLabel->setFont(infoFont);
        m_timeLabel->setFont(infoFont);

        infoLayout->addWidget(m_distanceLabel);
        infoLayout->addStretch();
        infoLayout->addWidget(m_timeLabel);

        // 核心导航按钮
        QPushButton *btnStartNav = new QPushButton("🚗 开始系统导航", this);
        btnStartNav->setMinimumHeight(45);
        btnStartNav->setStyleSheet("background-color: #1565C0; color: white; font-size: 16px; font-weight: bold; border-radius: 8px;");

        mainLayout->addLayout(modeLayout);
        mainLayout->addSpacing(10);
        mainLayout->addWidget(m_mapLabel, 1);
        mainLayout->addSpacing(10);
        mainLayout->addLayout(infoLayout);
        mainLayout->addWidget(btnStartNav);
        setCentralWidget(centralWidget);

        // ======== 交互1：出行方式联动 (改变 UI 颜色与数据) ========
        connect(btnDrive, &QRadioButton::toggled, this, [=](bool checked){
            if(checked) {
                m_currentMode = "car";
                m_mapLabel->setText("🗺️ 【导航系统监控区】\n\n📍 起点：当前位置\n📍 终点：东大一区充电站\n\n🚗 路线规划：驾车模式\n⚡ 状态：已为您避开拥堵路段");
                m_mapLabel->setStyleSheet("background-color: #E3F2FD; border: 2px dashed #3F51B5; border-radius: 12px; font-size: 16px; color: #303F9F; font-weight: bold;");
                m_distanceLabel->setText("距离：3.2 km");
                m_timeLabel->setText("预计耗时：10 分钟");
                m_timeLabel->setStyleSheet("color: #2E7D32;");
                btnStartNav->setText("🚗 开始驾车导航");
            }
        });

        connect(btnWalk, &QRadioButton::toggled, this, [=](bool checked){
            if(checked) {
                m_currentMode = "foot";
                m_mapLabel->setText("🗺️ 【导航系统监控区】\n\n📍 起点：当前位置\n📍 终点：东大一区充电站\n\n🚶 路线规划：步行模式\n🌿 状态：已为您选择最短路径");
                m_mapLabel->setStyleSheet("background-color: #FFF3E0; border: 2px dashed #FF9800; border-radius: 12px; font-size: 16px; color: #E65100; font-weight: bold;");
                m_distanceLabel->setText("距离：2.8 km");
                m_timeLabel->setText("预计耗时：40 分钟");
                m_timeLabel->setStyleSheet("color: #D84315;");
                btnStartNav->setText("🚶 开始步行导航");
            }
        });

        // ======== 交互2：拉起系统真实导航 ========
        connect(btnStartNav, &QPushButton::clicked, this, [=]() {
            // 构造 OpenStreetMap 真实路线规划 URL
            QString urlStr = QString("https://www.openstreetmap.org/directions?engine=graphhopper_%1&route=41.770%2C123.410%3B41.765%2C123.420")
                             .arg(m_currentMode);
            QDesktopServices::openUrl(QUrl(urlStr));
        });

        emit btnDrive->toggled(true);
    }

private:
    ev::PlatformClient *m_client;
    QLabel *m_mapLabel;
    QLabel *m_distanceLabel;
    QLabel *m_timeLabel;
    QString m_currentMode;
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ev::PlatformClient client("UserClient");
    UserMainWindow w(&client);
    w.show();
    return a.exec();
}
