#include "user_home_widget.h"
#include "client_ui/apple_widgets.h"
#include "client_ui/slide_toast.h"

#include "charge_flow_widget.h"
#include "membership_dialog.h"
#include "user_info_widget.h"
#include "station_search_widget.h"
#include "order_list_widget.h"
#include "user_api_client.h"

#include <QLabel>
#include <QButtonGroup>
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {

QIcon xiaoqingIcon()
{
    const QPixmap source(QStringLiteral(":/images/xiaoqing.png"));
    QPixmap pixmap(144, 144);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(QRectF(2, 2, 68, 68));
    painter.setClipPath(clip);
    const qreal inset = source.width() * 0.045;
    painter.drawPixmap(QRectF(2, 2, 68, 68), source,
        QRectF(inset, inset, source.width() - inset * 2, source.height() - inset * 2));
    return QIcon(pixmap);
}

class BottomNavigationBar final : public QWidget {
public:
    explicit BottomNavigationBar(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(100);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setCenterWidget(QWidget *widget)
    {
        centerWidget_ = widget;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal center = centerWidget_
            ? centerWidget_->geometry().center().x() : width() / 2.0;
        const qreal centerY = centerWidget_
            ? centerWidget_->geometry().top() + 37.0 : 38.0;
        const qreal barTop = centerY - 18.0;
        QPainterPath bar;
        bar.moveTo(1, barTop);
        bar.lineTo(width() - 1, barTop);
        bar.lineTo(width() - 1, height() - 17);
        bar.quadTo(width() - 1, height() - 1, width() - 17, height() - 1);
        bar.lineTo(17, height() - 1);
        bar.quadTo(1, height() - 1, 1, height() - 17);
        bar.closeSubpath();
        QPainterPath dome;
        dome.addEllipse(QPointF(center, centerY), 40, 40);
        const QPainterPath surface = bar.united(dome);
        painter.setPen(QPen(QColor("#e8ebf3"), 1));
        painter.setBrush(Qt::white);
        painter.drawPath(surface);
    }
private:
    QWidget *centerWidget_ = nullptr;
};

QIcon selectedNavigationIcon(ev::AppSymbol symbol)
{
    QPixmap pixmap(64, 64);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#5264d6"));
    switch(symbol){
    case ev::AppSymbol::Compass:
        painter.drawEllipse(QRectF(6.8,6.8,18.4,18.4));break;
    case ev::AppSymbol::Bolt:
        painter.drawPolygon(QPolygonF{{17.5,6},{9,17},{15,17},{14.5,26},{23,14},{17,14}});break;
    case ev::AppSymbol::Person:
        painter.drawEllipse(QRectF(12.8,7.8,6.4,6.4));painter.drawRoundedRect(QRectF(9,18,14,7),4,4);break;
    case ev::AppSymbol::Receipt:
        painter.drawRoundedRect(QRectF(9.8,6.8,12.4,18.4),1.5,1.5);break;
    default:break;
    }
    ev::drawAppSymbol(painter,QRectF(4,4,24,24),symbol,QColor("#202637"));
    return QIcon(pixmap);
}

QToolButton *navigationButton(const QString &text, ev::AppSymbol symbol,
                              QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setIcon(ev::appSymbolIcon(symbol));
    button->setIconSize(QSize(32, 32));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setProperty("uiClass", "bottomNavigation");
    button->setCheckable(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QObject::connect(button, &QToolButton::toggled, button,
                     [button, symbol](bool checked) {
        button->setIcon(checked ? selectedNavigationIcon(symbol)
                                : ev::appSymbolIcon(symbol));
    });
    return button;
}

} // namespace

UserHomeWidget::UserHomeWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 14, 24, 14);
    layout->setSpacing(16);
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("userTopBar"));
    auto *toolbar = new QHBoxLayout(header);
    toolbar->setContentsMargins(14, 5, 14, 5);
    toolbar->setSpacing(12);
    auto *brand = ev::makeAppleBrand(header);
    brand->setFixedWidth(142);
    toolbar->addWidget(brand);
    toolbar->addStretch();
    logoutButton_ = new QPushButton(QStringLiteral("退出登录"), header);
    logoutButton_->setProperty("uiClass", "text");
    toolbar->addWidget(logoutButton_);
    layout->addWidget(header);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(this, QStringLiteral("退出登录"),
            QStringLiteral("确定退出当前用户账号？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        setLogoutInProgress(true);
        emit logoutRequested();
    });
    auto *tabs = new QTabWidget(this);
    tabs_ = tabs;
    stationSearchWidget_ = new StationSearchWidget(api, tabs);
    chargeFlowWidget_ = new ChargeFlowWidget(api, tabs);
    connect(api, &ev::UserApiClient::sessionExpired, chargeFlowWidget_, &ChargeFlowWidget::reset);
    userInfoWidget_ = new UserInfoWidget(api, tabs);
    tabs->addTab(stationSearchWidget_, QStringLiteral("找桩"));
    tabs->addTab(chargeFlowWidget_, QStringLiteral("充电"));
    tabs->addTab(userInfoWidget_, QStringLiteral("个人中心"));
    orderListWidget_ = new OrderListWidget(api, tabs);
    tabs->addTab(orderListWidget_, QStringLiteral("我的订单"));
    connect(tabs, &QTabWidget::currentChanged, this, [this](int) {
        if (tabs_->currentWidget() == orderListWidget_) {
            orderListWidget_->refresh();
        } else {
            orderListWidget_->reset();
        }
    });
    connect(orderListWidget_, &OrderListWidget::backRequested, this, [this] {
        tabs_->setCurrentWidget(stationSearchWidget_);
    });
    tabs->tabBar()->hide();
    layout->addWidget(tabs, 1);

    auto *bottomBar = new BottomNavigationBar(this);
    bottomBar->setObjectName(QStringLiteral("userBottomBar"));
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(18, 4, 18, 5);
    bottomLayout->setSpacing(8);
    auto *stationButton = navigationButton(QStringLiteral("找桩"), ev::AppSymbol::Compass, bottomBar);
    auto *chargeButton = navigationButton(QStringLiteral("充电"), ev::AppSymbol::Bolt, bottomBar);
    auto *profileButton = navigationButton(QStringLiteral("个人中心"), ev::AppSymbol::Person, bottomBar);
    auto *ordersButton = navigationButton(QStringLiteral("我的订单"), ev::AppSymbol::Receipt, bottomBar);
    auto *assistantButton = new QToolButton(bottomBar);
    assistantButton->setObjectName(QStringLiteral("xiaoqingButton"));
    assistantButton->setAccessibleName(QStringLiteral("小轻 AI助手"));
    assistantButton->setText(QStringLiteral("小轻"));
    assistantButton->setIcon(xiaoqingIcon());
    assistantButton->setIconSize(QSize(68, 68));
    assistantButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    assistantButton->setFixedSize(112, 92);
    bottomBar->setCenterWidget(assistantButton);
    stationButton->setFixedHeight(74);
    chargeButton->setFixedHeight(74);
    profileButton->setFixedHeight(74);
    ordersButton->setFixedHeight(74);
    bottomLayout->addWidget(stationButton, 1, Qt::AlignBottom);
    bottomLayout->addWidget(chargeButton, 1, Qt::AlignBottom);
    bottomLayout->addWidget(assistantButton, 0, Qt::AlignBottom);
    bottomLayout->addWidget(profileButton, 1, Qt::AlignBottom);
    bottomLayout->addWidget(ordersButton, 1, Qt::AlignBottom);
    layout->addWidget(bottomBar);

    auto *navigationGroup = new QButtonGroup(this);
    navigationGroup->setExclusive(true);
    const QList<QToolButton *> navigationButtons {
        stationButton, chargeButton, profileButton, ordersButton
    };
    const QList<int> navigationPages {0, 1, 2, 3};
    for (int i = 0; i < navigationButtons.size(); ++i) {
        navigationGroup->addButton(navigationButtons.at(i), navigationPages.at(i));
        connect(navigationButtons.at(i), &QToolButton::clicked, this,
                [tabs, page = navigationPages.at(i)] { tabs->setCurrentIndex(page); });
    }
    connect(tabs, &QTabWidget::currentChanged, this,
            [navigationGroup](int index) {
        if (auto *button = navigationGroup->button(index)) {
            button->setChecked(true);
        }
    });
    stationButton->setChecked(true);
    connect(assistantButton, &QToolButton::clicked, this, [this, api] {
        auto *dialog = new MembershipDialog(api, true, this);
        dialog->show();
    });

    connect(stationSearchWidget_, &StationSearchWidget::pileChosen, this, [this, tabs](qint64 pileId) {
        chargeFlowWidget_->enterWithPile(pileId);
        tabs->setCurrentWidget(chargeFlowWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::pileSelectionRequested, this, [this, tabs] {
        tabs->setCurrentWidget(stationSearchWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::rechargeRequested, this, [this, tabs] {
        tabs->setCurrentWidget(userInfoWidget_);
    });
    connect(chargeFlowWidget_, &ChargeFlowWidget::homeRequested, this, [this, tabs] {
        tabs->setCurrentWidget(stationSearchWidget_);
    });
    // UML-025: entering the charge tab always re-checks pending orders first.
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        if (tabs->widget(index) == chargeFlowWidget_) {
            chargeFlowWidget_->enterFromHome();
        }
    });
}

void UserHomeWidget::refresh()
{
    orderListWidget_->reset();
    chargeFlowWidget_->reset();
    tabs_->setCurrentWidget(stationSearchWidget_);
    stationSearchWidget_->refresh();
    userInfoWidget_->refreshFromSession();
}

void UserHomeWidget::showWelcome(bool isNewUser)
{
    ev::SlideToast::show(window(),isNewUser
        ? QStringLiteral("注册成功，欢迎加入！")
        : QStringLiteral("登录成功"));
}

void UserHomeWidget::setLogoutInProgress(bool inProgress)
{
    logoutButton_->setEnabled(!inProgress);
    logoutButton_->setText(inProgress ? QStringLiteral("正在退出...")
                                      : QStringLiteral("退出登录"));
}
