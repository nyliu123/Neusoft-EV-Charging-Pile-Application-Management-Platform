#pragma once

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
namespace ev { class UserApiClient; }

class OrderListWidget final : public QWidget {
    Q_OBJECT
public:
    explicit OrderListWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refresh();
    void reset();

signals:
    void backRequested();

private:
    void clearCards();
    void renderOrders(const QJsonArray &orders);

    ev::UserApiClient *api_;
    QLabel *statusLabel_;
    QPushButton *refreshButton_;
    QVBoxLayout *cardsLayout_;
    quint64 requestGeneration_ = 0;
};
