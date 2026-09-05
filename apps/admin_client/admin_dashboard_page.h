#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;

namespace ev {

class AdminApiClient;
class LineChartWidget;
class PieChartWidget;
class StatCard;

// UML-035/036/037: stat cards, 7-day revenue trend line chart and pile status
// distribution, plus a fault-pile warning bar.
class AdminDashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminDashboardPage(AdminApiClient *api, QWidget *parent = nullptr);

    void reload();

signals:
    void pileManagementRequested();

private:
    void applySummary(bool ok, const QJsonObject &result, const QString &message);
    void applyTrend(bool ok, const QJsonObject &result, const QString &message);
    void applyStats(bool ok, const QJsonObject &result, const QString &message);
    void noteError(const QString &message);
    void updateErrorLabel();

    AdminApiClient *api_ = nullptr;

    StatCard *kwhCard_ = nullptr;
    StatCard *revenueCard_ = nullptr;
    StatCard *userCard_ = nullptr;
    StatCard *orderCard_ = nullptr;
    LineChartWidget *trendChart_ = nullptr;
    PieChartWidget *pileChart_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QWidget *faultBar_ = nullptr;
    QLabel *faultWarningLabel_ = nullptr;
    QPushButton *faultJumpButton_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QStringList errors_;
};

} // namespace ev
