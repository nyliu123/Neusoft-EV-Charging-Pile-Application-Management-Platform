#include "admin_dashboard_page.h"

#include "admin_api_client.h"
#include "admin_charts.h"
#include "admin_format.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ev {

namespace {

QWidget *chartCard(const QString &title, QWidget *chart, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: white; border: 1px solid #e0e6ed; border-radius: 8px; }"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(8);

    auto *label = new QLabel(title, card);
    QFont labelFont = label->font();
    labelFont.setBold(true);
    label->setFont(labelFont);
    label->setStyleSheet(QStringLiteral(
        "color: #424242; border: none; background: transparent;"));
    layout->addWidget(label);
    layout->addWidget(chart, 1);
    return card;
}

} // namespace

AdminDashboardPage::AdminDashboardPage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(16);

    // Header.
    auto *headerLayout = new QHBoxLayout();
    auto *titleLabel = new QLabel(QStringLiteral("经营看板"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    headerLayout->addWidget(refreshButton_);
    rootLayout->addLayout(headerLayout);

    // Fault warning bar (UML-037).
    faultBar_ = new QWidget(this);
    faultBar_->setStyleSheet(QStringLiteral(
        "background: #fff3e0; border: 1px solid #ffe0b2; border-radius: 6px;"));
    auto *faultLayout = new QHBoxLayout(faultBar_);
    faultLayout->setContentsMargins(12, 6, 12, 6);
    faultWarningLabel_ = new QLabel(faultBar_);
    faultWarningLabel_->setStyleSheet(QStringLiteral(
        "color: #e65100; border: none; background: transparent;"));
    faultJumpButton_ = new QPushButton(QStringLiteral("前往处理"), faultBar_);
    faultJumpButton_->setFlat(true);
    faultJumpButton_->setCursor(Qt::PointingHandCursor);
    faultJumpButton_->setStyleSheet(QStringLiteral(
        "color: #1565c0; border: none; text-decoration: underline;"));
    faultLayout->addWidget(faultWarningLabel_);
    faultLayout->addStretch();
    faultLayout->addWidget(faultJumpButton_);
    faultBar_->setVisible(false);
    rootLayout->addWidget(faultBar_);

    // Stat cards row (UML-035).
    auto *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);
    kwhCard_ = new StatCard(QStringLiteral("累计充电量（度）"), QColor(0x19, 0x76, 0xd2), this);
    revenueCard_ = new StatCard(QStringLiteral("累计营收（元）"), QColor(0x2e, 0x7d, 0x32), this);
    userCard_ = new StatCard(QStringLiteral("注册用户数"), QColor(0xf9, 0xa8, 0x25), this);
    orderCard_ = new StatCard(QStringLiteral("累计充电次数"), QColor(0x6a, 0x1b, 0x9a), this);
    cardsLayout->addWidget(kwhCard_, 1);
    cardsLayout->addWidget(revenueCard_, 1);
    cardsLayout->addWidget(userCard_, 1);
    cardsLayout->addWidget(orderCard_, 1);
    rootLayout->addLayout(cardsLayout);

    // Charts row (UML-036/037).
    trendChart_ = new LineChartWidget(this);
    pileChart_ = new PieChartWidget(this);
    rootLayout->addWidget(chartCard(QStringLiteral("近 7 日营收趋势（元）"),
                                    trendChart_, this), 3);
    rootLayout->addWidget(chartCard(QStringLiteral("充电桩状态分布"),
                                    pileChart_, this), 2);

    // Error line.
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet(QStringLiteral("color: #c62828; font-size: 12px;"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    rootLayout->addWidget(errorLabel_);

    connect(refreshButton_, &QPushButton::clicked, this, &AdminDashboardPage::reload);
    connect(faultJumpButton_, &QPushButton::clicked,
            this, &AdminDashboardPage::pileManagementRequested);
}

void AdminDashboardPage::reload()
{
    errors_.clear();
    updateErrorLabel();

    api_->sendQuery(QStringLiteral("dashboard_summary"), QJsonObject {}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            applySummary(ok, result, message);
        });
    api_->sendQuery(QStringLiteral("revenue_trend"),
        QJsonObject {{QStringLiteral("days"), 7}}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            applyTrend(ok, result, message);
        });
    api_->sendQuery(QStringLiteral("pile_status_stats"), QJsonObject {}, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            applyStats(ok, result, message);
        });
}

void AdminDashboardPage::applySummary(bool ok, const QJsonObject &result,
                                      const QString &message)
{
    if (!ok) {
        noteError(QStringLiteral("核心指标加载失败：%1").arg(message));
        return;
    }
    kwhCard_->setValue(formatAmount(result.value(QStringLiteral("total_kwh")).toDouble()));
    revenueCard_->setValue(
        QStringLiteral("¥ %1").arg(formatAmount(result.value(QStringLiteral("total_revenue")).toDouble())));
    userCard_->setValue(QString::number(result.value(QStringLiteral("total_users")).toInt()));
    orderCard_->setValue(QString::number(result.value(QStringLiteral("total_orders")).toInt()));
}

void AdminDashboardPage::applyTrend(bool ok, const QJsonObject &result,
                                    const QString &message)
{
    if (!ok) {
        noteError(QStringLiteral("营收趋势加载失败：%1").arg(message));
        return;
    }
    QVector<QPair<QString, double>> points;
    const QJsonArray rawPoints = result.value(QStringLiteral("points")).toArray();
    for (const QJsonValue &value : rawPoints) {
        const QJsonObject point = value.toObject();
        // Show "MM-dd" under the axis.
        points.append({point.value(QStringLiteral("date")).toString().mid(5),
                       point.value(QStringLiteral("revenue")).toDouble()});
    }
    trendChart_->setPoints(points, QStringLiteral(" 元"));
}

void AdminDashboardPage::applyStats(bool ok, const QJsonObject &result,
                                    const QString &message)
{
    if (!ok) {
        noteError(QStringLiteral("充电桩状态加载失败：%1").arg(message));
        return;
    }
    QVector<PieChartWidget::Slice> slices;
    int faultCount = 0;
    const QJsonArray stats = result.value(QStringLiteral("stats")).toArray();
    for (const QJsonValue &value : stats) {
        const QJsonObject entry = value.toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        const int count = entry.value(QStringLiteral("count")).toInt();
        slices.append({entry.value(QStringLiteral("label")).toString(),
                       count, pileStatusColor(status)});
        if (status == QStringLiteral("fault")) {
            faultCount = count;
        }
    }
    pileChart_->setSlices(slices);

    faultBar_->setVisible(faultCount > 0);
    if (faultCount > 0) {
        faultWarningLabel_->setText(
            QStringLiteral("当前有 %1 台充电桩处于故障状态，请及时处理。").arg(faultCount));
    }
}

void AdminDashboardPage::noteError(const QString &message)
{
    errors_.append(message);
    updateErrorLabel();
}

void AdminDashboardPage::updateErrorLabel()
{
    if (errors_.isEmpty()) {
        errorLabel_->setVisible(false);
        return;
    }
    errorLabel_->setText(errors_.join(QStringLiteral("；")));
    errorLabel_->setVisible(true);
}

} // namespace ev
