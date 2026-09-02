#include "user_mainwindow.h"
#include "ui_text.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtMath>

#include <QtWebEngineWidgets/QWebEngineView>

namespace evcs::userclient {

namespace {

QTableWidgetItem *makeIdItem(qint64 id)
{
    auto *item = new QTableWidgetItem(QString::number(id));
    item->setData(Qt::UserRole, id);
    return item;
}

QString moneyText(qint64 cents)
{
    return QStringLiteral("¥%1").arg(static_cast<double>(cents) / 100.0, 0, 'f', 2);
}

QString localTimeText(const QString &isoText)
{
    if (isoText.isEmpty()) return {};
    const QDateTime time = QDateTime::fromString(isoText, Qt::ISODateWithMs);
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : isoText;
}

} // namespace

// 本文件统一解析服务端响应，并把业务数据刷新到对应页面。
void MainWindow::handleResponse(const QString &action,
                                bool ok,
                                const QJsonObject &data,
                                const QString &errorCode,
                                const QString &errorMessage)
{
    if (!ok) {
        if (action == QStringLiteral("charging.status") && errorCode == QStringLiteral("NOT_FOUND")) {
            chargingTimer_.stop();
            return;
        }
        QMessageBox::warning(this, QStringLiteral("操作失败"), errorMessage);
        return;
    }

    if (action == QStringLiteral("auth.login")
        || action == QStringLiteral("auth.phoneLogin")) {
        apiClient_.setToken(data.value(QStringLiteral("token")).toString());
        const QJsonObject user = data.value(QStringLiteral("user")).toObject();
        if (user.value(QStringLiteral("role")).toString() != QStringLiteral("user")) {
            apiClient_.clearToken();
            QMessageBox::warning(this, QStringLiteral("角色不匹配"),
                                 QStringLiteral("请使用普通用户账号登录用户端"));
            return;
        }
        statusBar()->showMessage(QStringLiteral("当前用户：%1，余额 %2")
            .arg(user.value(QStringLiteral("displayName")).toString(),
                 moneyText(static_cast<qint64>(user.value(QStringLiteral("balanceCents")).toDouble()))));
        stack_->setCurrentWidget(tabs_);
        refreshStations();
        refreshReservations();
        refreshOrders();
        refreshProfile();
        refreshChargingStatus();
        if (data.value(QStringLiteral("autoRegistered")).toBool(false)) {
            QMessageBox::information(this, QStringLiteral("自动注册成功"),
                                     QStringLiteral("该手机号首次登录，已自动创建用户"));
        }
    } else if (action == QStringLiteral("auth.register")) {
        QMessageBox::information(this, QStringLiteral("注册成功"),
                                 QStringLiteral("用户已创建，可以登录"));
    } else if (action == QStringLiteral("station.list")) {
        populateStations(data);
    } else if (action == QStringLiteral("station.get")) {
        populateChargers(data);
    } else if (action == QStringLiteral("reservation.create")) {
        QMessageBox::information(this, QStringLiteral("预约成功"),
                                 QStringLiteral("预约有效期 15 分钟"));
        refreshReservations();
        refreshStations();
    } else if (action == QStringLiteral("reservation.cancel")) {
        refreshReservations();
        refreshStations();
    } else if (action == QStringLiteral("reservation.list")) {
        populateReservations(data);
    } else if (action == QStringLiteral("charging.start")) {
        activeSessionId_ = static_cast<qint64>(data.value(QStringLiteral("sessionId")).toDouble());
        tabs_->setCurrentIndex(2);
        stopChargingButton_->setEnabled(true);
        chargingTimer_.start();
        refreshChargingStatus();
        refreshReservations();
    } else if (action == QStringLiteral("charging.status")) {
        populateCharging(data);
    } else if (action == QStringLiteral("charging.stop")) {
        chargingTimer_.stop();
        activeSessionId_ = 0;
        stopChargingButton_->setEnabled(false);
        const QJsonObject order = data.value(QStringLiteral("order")).toObject();
        QMessageBox::information(this, QStringLiteral("结算完成"),
            QStringLiteral("订单 %1\n电量 %2 kWh\n金额 %3\n状态 %4")
                .arg(order.value(QStringLiteral("orderNo")).toString())
                .arg(order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble())))
                .arg(statusText(order.value(QStringLiteral("status")).toString())));
        tabs_->setCurrentIndex(3);
        refreshOrders();
        refreshStations();
    } else if (action == QStringLiteral("order.list")) {
        populateOrders(data);
    } else if (action == QStringLiteral("order.get")) {
        const QJsonObject order = data.value(QStringLiteral("order")).toObject();
        QMessageBox::information(this, QStringLiteral("订单明细"),
            QStringLiteral("订单号：%1\n站点：%2\n地址：%3\n充电桩：%4\n"
                           "开始：%5\n结束：%6\n电量：%7 kWh\n单价：%8/kWh\n金额：%9\n状态：%10")
                .arg(order.value(QStringLiteral("orderNo")).toString(),
                     order.value(QStringLiteral("stationName")).toString(),
                     order.value(QStringLiteral("stationAddress")).toString(),
                     order.value(QStringLiteral("chargerCode")).toString(),
                     localTimeText(order.value(QStringLiteral("startedAt")).toString()),
                     localTimeText(order.value(QStringLiteral("endedAt")).toString()))
                .arg(order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("priceCentsPerKwh")).toDouble())))
                .arg(moneyText(static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble())))
                .arg(statusText(order.value(QStringLiteral("status")).toString())));
    } else if (action == QStringLiteral("user.profile")) {
        populateProfile(data);
    } else if (action == QStringLiteral("user.profile.update")) {
        QMessageBox::information(this, QStringLiteral("修改成功"),
                                 QStringLiteral("昵称已保存"));
        refreshProfile();
    } else if (action == QStringLiteral("user.avatar.update")) {
        QMessageBox::information(this, QStringLiteral("上传成功"),
                                 QStringLiteral("头像已保存"));
        refreshProfile();
    } else if (action == QStringLiteral("wallet.recharge")) {
        QMessageBox::information(this, QStringLiteral("充值成功"),
            QStringLiteral("模拟充值完成，当前余额 %1")
                .arg(moneyText(static_cast<qint64>(
                    data.value(QStringLiteral("balanceCents")).toDouble()))));
        refreshProfile();
    } else if (action == QStringLiteral("auth.logout")) {
        apiClient_.clearToken();
        activeSessionId_ = 0;
        stopChargingButton_->setEnabled(false);
        stack_->setCurrentWidget(loginPage_);
        statusBar()->showMessage(QStringLiteral("已退出登录"), 5000);
    }
}

void MainWindow::populateStations(const QJsonObject &data)
{
    const QJsonArray stations = data.value(QStringLiteral("stations")).toArray();
    stationTable_->setRowCount(stations.size());
    for (int row = 0; row < stations.size(); ++row) {
        const QJsonObject station = stations.at(row).toObject();
        const qint64 id = static_cast<qint64>(station.value(QStringLiteral("id")).toDouble());
        auto *idItem = makeIdItem(id);
        idItem->setData(Qt::UserRole + 1, station.value(QStringLiteral("latitude")).toDouble());
        idItem->setData(Qt::UserRole + 2, station.value(QStringLiteral("longitude")).toDouble());
        stationTable_->setItem(row, 0, idItem);
        stationTable_->setItem(row, 1, new QTableWidgetItem(station.value(QStringLiteral("name")).toString()));
        stationTable_->setItem(row, 2, new QTableWidgetItem(station.value(QStringLiteral("region")).toString()));
        stationTable_->setItem(row, 3, new QTableWidgetItem(station.value(QStringLiteral("address")).toString()));
        stationTable_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1/%2")
            .arg(station.value(QStringLiteral("idleCount")).toInt())
            .arg(station.value(QStringLiteral("chargerCount")).toInt())));
        stationTable_->setItem(row, 5, new QTableWidgetItem(QStringLiteral("%1%")
            .arg(station.value(QStringLiteral("onlineRate")).toDouble() * 100.0, 0, 'f', 1)));
        stationTable_->setItem(row, 6, new QTableWidgetItem(
            station.contains(QStringLiteral("distanceKm"))
                ? QStringLiteral("%1 km").arg(
                    station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 2)
                : QStringLiteral("--")));
        stationTable_->setItem(row, 7, new QTableWidgetItem(
            moneyText(station.value(QStringLiteral("minimumPriceCentsPerKwh")).toInt())
            + QStringLiteral("/kWh")));
    }
    if (!stations.isEmpty()) stationTable_->selectRow(0);
}

void MainWindow::populateChargers(const QJsonObject &data)
{
    const QJsonArray chargers = data.value(QStringLiteral("station")).toObject()
                                    .value(QStringLiteral("chargers")).toArray();
    chargerTable_->setRowCount(chargers.size());
    for (int row = 0; row < chargers.size(); ++row) {
        const QJsonObject charger = chargers.at(row).toObject();
        const qint64 id = static_cast<qint64>(charger.value(QStringLiteral("id")).toDouble());
        chargerTable_->setItem(row, 0, makeIdItem(id));
        chargerTable_->setItem(row, 1, new QTableWidgetItem(charger.value(QStringLiteral("code")).toString()));
        chargerTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 kW").arg(
            charger.value(QStringLiteral("ratedPowerKw")).toDouble(), 0, 'f', 1)));
        chargerTable_->setItem(row, 3, new QTableWidgetItem(
            statusText(charger.value(QStringLiteral("status")).toString())));
        chargerTable_->setItem(row, 4, new QTableWidgetItem(
            moneyText(charger.value(QStringLiteral("priceCentsPerKwh")).toInt())
            + QStringLiteral("/kWh")));
    }
    if (!chargers.isEmpty()) chargerTable_->selectRow(0);
}

void MainWindow::populateReservations(const QJsonObject &data)
{
    const QJsonArray reservations = data.value(QStringLiteral("reservations")).toArray();
    reservationTable_->setRowCount(reservations.size());
    for (int row = 0; row < reservations.size(); ++row) {
        const QJsonObject reservation = reservations.at(row).toObject();
        const qint64 id = static_cast<qint64>(reservation.value(QStringLiteral("id")).toDouble());
        reservationTable_->setItem(row, 0, makeIdItem(id));
        reservationTable_->setItem(row, 1, new QTableWidgetItem(reservation.value(QStringLiteral("stationName")).toString()));
        reservationTable_->setItem(row, 2, new QTableWidgetItem(reservation.value(QStringLiteral("chargerCode")).toString()));
        reservationTable_->setItem(row, 3, new QTableWidgetItem(
            statusText(reservation.value(QStringLiteral("status")).toString())));
        reservationTable_->setItem(row, 4, new QTableWidgetItem(localTimeText(reservation.value(QStringLiteral("reservedAt")).toString())));
        reservationTable_->setItem(row, 5, new QTableWidgetItem(localTimeText(reservation.value(QStringLiteral("expiresAt")).toString())));
    }
    if (!reservations.isEmpty()) reservationTable_->selectRow(0);
}

void MainWindow::populateCharging(const QJsonObject &data)
{
    const QJsonObject session = data.value(QStringLiteral("session")).toObject();
    activeSessionId_ = static_cast<qint64>(session.value(QStringLiteral("id")).toDouble());
    chargingStationLabel_->setText(session.value(QStringLiteral("stationName")).toString());
    chargingChargerLabel_->setText(session.value(QStringLiteral("chargerCode")).toString());
    chargingTimeLabel_->setText(QStringLiteral("%1 秒").arg(
        static_cast<qint64>(session.value(QStringLiteral("elapsedSeconds")).toDouble())));
    chargingEnergyLabel_->setText(QStringLiteral("%1 kWh").arg(
        session.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3));
    chargingAmountLabel_->setText(moneyText(
        static_cast<qint64>(session.value(QStringLiteral("amountCents")).toDouble())));
    const bool charging = session.value(QStringLiteral("status")).toString() == QStringLiteral("charging");
    stopChargingButton_->setEnabled(charging);
    if (charging && !chargingTimer_.isActive()) chargingTimer_.start();
}

void MainWindow::populateOrders(const QJsonObject &data)
{
    const QJsonArray orders = data.value(QStringLiteral("orders")).toArray();
    orderTable_->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const QJsonObject order = orders.at(row).toObject();
        const qint64 id = static_cast<qint64>(order.value(QStringLiteral("id")).toDouble());
        orderTable_->setItem(row, 0, makeIdItem(id));
        orderTable_->setItem(row, 1, new QTableWidgetItem(order.value(QStringLiteral("orderNo")).toString()));
        orderTable_->setItem(row, 2, new QTableWidgetItem(order.value(QStringLiteral("stationName")).toString()));
        orderTable_->setItem(row, 3, new QTableWidgetItem(order.value(QStringLiteral("chargerCode")).toString()));
        orderTable_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1 kWh").arg(
            order.value(QStringLiteral("energyWh")).toDouble() / 1000.0, 0, 'f', 3)));
        orderTable_->setItem(row, 5, new QTableWidgetItem(moneyText(
            static_cast<qint64>(order.value(QStringLiteral("amountCents")).toDouble()))));
        orderTable_->setItem(row, 6, new QTableWidgetItem(
            statusText(order.value(QStringLiteral("status")).toString())));
        orderTable_->setItem(row, 7, new QTableWidgetItem(localTimeText(order.value(QStringLiteral("createdAt")).toString())));
    }
}

void MainWindow::populateProfile(const QJsonObject &data)
{
    const QJsonObject user = data.value(QStringLiteral("user")).toObject();
    const QByteArray avatarData = QByteArray::fromBase64(
        user.value(QStringLiteral("avatarBase64")).toString().toLatin1());
    QImage avatar;
    if (!avatarData.isEmpty()) avatar.loadFromData(avatarData);
    if (avatar.isNull()) {
        profileAvatarLabel_->setPixmap({});
        profileAvatarLabel_->setText(QStringLiteral("暂无头像"));
    } else {
        profileAvatarLabel_->setText({});
        profileAvatarLabel_->setPixmap(QPixmap::fromImage(avatar).scaled(
            profileAvatarLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    profileUsernameLabel_->setText(user.value(QStringLiteral("username")).toString());
    profileDisplayNameLabel_->setText(user.value(QStringLiteral("displayName")).toString());
    profilePhoneLabel_->setText(user.value(QStringLiteral("phone")).toString());
    profileBalanceLabel_->setText(moneyText(
        static_cast<qint64>(user.value(QStringLiteral("balanceCents")).toDouble())));
    profileCreatedAtLabel_->setText(localTimeText(user.value(QStringLiteral("createdAt")).toString()));
}

} // namespace evcs::userclient
