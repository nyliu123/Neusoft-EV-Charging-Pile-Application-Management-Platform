#pragma once

#include <QJsonObject>
#include <QWidget>
#include <QVector>

class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace ev {
class UserApiClient;
}

// UML-025~032 charging flow UI: one stacked page per flow stage —
// pending-order check → pile confirm → charge progress (server pushed)
// → settlement result. Fees are kept in integer cents on the wire and
// rendered as yuan here.
class ChargeFlowWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ChargeFlowWidget(ev::UserApiClient *api, QWidget *parent = nullptr);

    // Entry from the station detail page with a chosen pile (UML-022 → 025/026).
    void enterWithPile(qint64 pileId);
    // Entry from the home tab without a pile: runs the UML-025 check first.
    // Ignored while a flow is already active.
    void enterFromHome();
    // Drop all flow state (used on logout).
    void reset();

signals:
    // No pending order and no pile chosen yet: switch to the find-pile tab.
    void pileSelectionRequested();
    // Insufficient balance: switch to the wallet/recharge tab (UML-017).
    void rechargeRequested();
    // Flow finished or cancelled: return to the home tab.
    void homeRequested();

private:
    enum class Stage {
        Idle,
        Checking,
        SelectPile,
        Reserved,
        Charging,
        Settled,
        PendingSettlement
    };

    void setStage(Stage stage);
    void runPendingCheck(qint64 pileId);
    void verifyPile(qint64 pileId);
    void reservePile();
    void startCharging();
    void finishCharging();
    void retrySettlement();
    void cancelReservation();
    void applyPendingOrder(const QJsonObject &order);
    void onChargeUpdate(const QJsonObject &update);
    void showReservedPage();
    void showChargingPage();
    void showSettledPage(const QJsonObject &result);
    void showPendingPage(const QJsonObject &result);
    void renderPileInfo(QLabel *infoLabel, QLabel *priceLabel) const;
    void updateElapsedLabel();

    ev::UserApiClient *api_ = nullptr;
    QWidget *stepBar_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QWidget *idlePage_ = nullptr;
    QLabel *idleLabel_ = nullptr;
    QWidget *selectPage_ = nullptr;
    QLabel *selectInfo_ = nullptr;
    QLabel *selectPrice_ = nullptr;
    QPushButton *reserveButton_ = nullptr;
    QPushButton *reselectButton_ = nullptr;
    QWidget *progressPage_ = nullptr;
    QLabel *progressTitle_ = nullptr;
    QLabel *progressInfo_ = nullptr;
    QLabel *progressPrice_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QFrame *metricsCard_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QLabel *kwhLabel_ = nullptr;
    QLabel *feeLabel_ = nullptr;
    QLabel *elapsedLabel_ = nullptr;
    QPushButton *startButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    QPushButton *endButton_ = nullptr;
    QWidget *settlePage_ = nullptr;
    QLabel *settleTitle_ = nullptr;
    QLabel *settleDetail_ = nullptr;
    QPushButton *homeButton_ = nullptr;
    QWidget *pendingPage_ = nullptr;
    QLabel *pendingTitle_ = nullptr;
    QLabel *pendingDetail_ = nullptr;
    QPushButton *rechargeButton_ = nullptr;
    QPushButton *retryButton_ = nullptr;

    QTimer *elapsedTimer_ = nullptr;
    Stage stage_ = Stage::Idle;
    qint64 orderId_ = 0;
    qint64 pileId_ = 0;
    QString startTimeText_;
    QString stationName_;
    QString pileNumber_;
    QString pileType_;
    double powerKw_ = 0.0;
    double pricePerKwh_ = 0.0;
    QString membershipLevel_ = QStringLiteral("NORMAL");
    int discountBps_ = 10000;
};
