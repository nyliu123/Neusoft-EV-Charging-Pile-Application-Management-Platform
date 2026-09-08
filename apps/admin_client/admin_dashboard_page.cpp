#include "admin_dashboard_page.h"

#include "admin_api_client.h"
#include "admin_charts.h"
#include "admin_format.h"

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
    card->setProperty("uiClass", "card");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(8);

    auto *label = new QLabel(title, card);
    label->setProperty("uiClass", "cardTitle");
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
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setProperty("uiClass", "secondary");
    headerLayout->addWidget(refreshButton_);
    rootLayout->addLayout(headerLayout);

    // Fault warning bar (UML-037).
    faultBar_ = new QWidget(this);
    faultBar_->setObjectName(QStringLiteral("faultBanner"));
    auto *faultLayout = new QHBoxLayout(faultBar_);
    faultLayout->setContentsMargins(12, 6, 12, 6);
    faultWarningLabel_ = new QLabel(faultBar_);
    faultJumpButton_ = new QPushButton(QStringLiteral("前往处理"), faultBar_);
    faultJumpButton_->setFlat(true);
    faultJumpButton_->setProperty("uiClass", "text");
    faultJumpButton_->setCursor(Qt::PointingHandCursor);
    faultLayout->addWidget(faultWarningLabel_);
    faultLayout->addStretch();
    faultLayout->addWidget(faultJumpButton_);
    faultBar_->setVisible(false);
    rootLayout->addWidget(faultBar_);

    // Stat cards row (UML-035).
    auto *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);
    kwhCard_ = new StatCard(QStringLiteral("累计充电量（度）"), QStringLiteral("blue"), this);
    revenueCard_ = new StatCard(QStringLiteral("累计营收（元）"), QStringLiteral("green"), this);
    userCard_ = new StatCard(QStringLiteral("注册用户数"), QStringLiteral("amber"), this);
    orderCard_ = new StatCard(QStringLiteral("累计充电次数"), QStringLiteral("violet"), this);
    cardsLayout->addWidget(kwhCard_, 1);
    cardsLayout->addWidget(revenueCard_, 1);
    cardsLayout->addWidget(userCard_, 1);
    cardsLayout->addWidget(orderCard_, 1);
    rootLayout->addLayout(cardsLayout);

    // Charts row (UML-036/037).
    trendChart_ = new LineChartWidget(this);
    pileChart_ = new PieChartWidget(this);
    auto *chartsLayout = new QHBoxLayout;
    chartsLayout->setSpacing(16);
    chartsLayout->addWidget(chartCard(QStringLiteral("近 7 日营收趋势（元）"),
                                     trendChart_, this), 3);
    chartsLayout->addWidget(chartCard(QStringLiteral("充电桩状态分布"),
                                     pileChart_, this), 2);
    trendChart_->setMinimumHeight(250);
    pileChart_->setMinimumHeight(250);
    rootLayout->addLayout(chartsLayout, 1);
    auto *footnote = new QLabel(QStringLiteral("运营数据由服务端同步 · 金额与充电量以实际订单为准"), this);
    footnote->setProperty("uiClass", "muted");
    rootLayout->addWidget(footnote);

    // Error line.
    errorLabel_ = new QLabel(this);
    errorLabel_->setProperty("uiClass", "errorText");
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

    api_->sendQuery(QStringLiteral("dashboard_overview"),
        QJsonObject {{QStringLiteral("days"), 7}}, this,
        [this](bool ok, const QJsonObject &result, const QString &) {
            if (!ok) {
                // Keep compatibility with an already-running older server.
                loadLegacyDashboard();
                return;
            }
            applySummary(true, result.value(QStringLiteral("summary")).toObject(), {});
            applyTrend(true, result.value(QStringLiteral("trend")).toObject(), {});
            applyStats(true, result.value(QStringLiteral("stats")).toObject(), {});
        });
}

void AdminDashboardPage::loadLegacyDashboard()
{
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
                       static_cast<double>(count),
                       status == QStringLiteral("idle") ? QColor("#72bba3")
                       : status == QStringLiteral("in_use") ? QColor("#7c8cda")
                       : status == QStringLiteral("reserved") ? QColor("#e5be78")
                       : status == QStringLiteral("fault") ? QColor("#d78b98")
                       : pileStatusColor(status)});
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
