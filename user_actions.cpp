#include "user_mainwindow.h"

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
#include <QRadioButton>
#include <QRegularExpression>
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

double straightLineDistanceKm(double latitude1, double longitude1,
                              double latitude2, double longitude2)
{
    constexpr double earthRadiusKm = 6371.0088;
    const double lat1 = qDegreesToRadians(latitude1);
    const double lat2 = qDegreesToRadians(latitude2);
    const double deltaLat = qDegreesToRadians(latitude2 - latitude1);
    const double deltaLon = qDegreesToRadians(longitude2 - longitude1);
    const double a = qSin(deltaLat / 2.0) * qSin(deltaLat / 2.0)
        + qCos(lat1) * qCos(lat2) * qSin(deltaLon / 2.0) * qSin(deltaLon / 2.0);
    return earthRadiusKm * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
}

} // namespace

// 本文件处理用户操作、地图请求和发往服务端的业务请求。
void MainWindow::connectSignals()
{
    connect(&apiClient_, &ApiClient::connectionChanged, this,
            [this](bool connected, const QString &message) {
        connectionLabel_->setText(message);
        loginButton_->setEnabled(connected);
        statusBar()->showMessage(message);
        if (!connected) {
            chargingTimer_.stop();
        } else if (!apiClient_.token().isEmpty()) {
            refreshStations();
            refreshReservations();
            refreshOrders();
            refreshProfile();
            refreshChargingStatus();
        }
    });
    connect(&apiClient_, &ApiClient::responseReceived, this,
            [this](const QString &, const QString &action, bool ok, const QJsonObject &data,
                   const QString &errorCode, const QString &errorMessage) {
        handleResponse(action, ok, data, errorCode, errorMessage);
    });
}

void MainWindow::connectServer()
{
    apiClient_.connectToServer(hostEdit_->text().trimmed(),
                               static_cast<quint16>(portSpin_->value()));
}

void MainWindow::login()
{
    if (!apiClient_.isConnected()) {
        QMessageBox::warning(this, QStringLiteral("未连接"), QStringLiteral("请先连接服务端"));
        return;
    }
    if (loginModeCombo_->currentData().toString() == QStringLiteral("phone")) {
        const QString phone = usernameEdit_->text().trimmed();
        static const QRegularExpression phonePattern(QStringLiteral("^1[3-9][0-9]{9}$"));
        if (!phonePattern.match(phone).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("手机号格式错误"),
                                 QStringLiteral("请输入 11 位中国大陆手机号"));
            return;
        }
        apiClient_.sendRequest(QStringLiteral("auth.phoneLogin"), {
            {QStringLiteral("phone"), phone}
        });
    } else {
        apiClient_.sendRequest(QStringLiteral("auth.login"), {
            {QStringLiteral("username"), usernameEdit_->text().trimmed()},
            {QStringLiteral("password"), passwordEdit_->text()}
        });
    }
}

void MainWindow::updateLoginMode()
{
    const bool phoneMode = loginModeCombo_->currentData().toString() == QStringLiteral("phone");
    passwordEdit_->setVisible(!phoneMode);
    usernameEdit_->setPlaceholderText(phoneMode
        ? QStringLiteral("11 位手机号，例如 13800138000")
        : QStringLiteral("用户名"));
    if (phoneMode && usernameEdit_->text() == QStringLiteral("demo")) {
        usernameEdit_->clear();
    } else if (!phoneMode && usernameEdit_->text().isEmpty()) {
        usernameEdit_->setText(QStringLiteral("demo"));
    }
}

void MainWindow::registerUser()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("注册用户"));
    QFormLayout form(&dialog);
    QLineEdit username;
    QLineEdit password;
    password.setEchoMode(QLineEdit::Password);
    QLineEdit displayName;
    QLineEdit phone;
    form.addRow(QStringLiteral("用户名"), &username);
    form.addRow(QStringLiteral("密码"), &password);
    form.addRow(QStringLiteral("姓名"), &displayName);
    form.addRow(QStringLiteral("手机号"), &phone);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    apiClient_.sendRequest(QStringLiteral("auth.register"), {
        {QStringLiteral("username"), username.text().trimmed()},
        {QStringLiteral("password"), password.text()},
        {QStringLiteral("displayName"), displayName.text().trimmed()},
        {QStringLiteral("phone"), phone.text().trimmed()}
    });
}

void MainWindow::refreshStations()
{
    QJsonObject payload{
        {QStringLiteral("keyword"), stationKeywordEdit_->text().trimmed()},
        {QStringLiteral("region"), stationRegionEdit_->text().trimmed()},
        {QStringLiteral("onlyAvailable"), onlyAvailableCheck_->isChecked()}
    };
    if (hasCurrentLocation_) {
        payload.insert(QStringLiteral("latitude"), currentLatitude_);
        payload.insert(QStringLiteral("longitude"), currentLongitude_);
    }
    apiClient_.sendRequest(QStringLiteral("station.list"), payload);
}

void MainWindow::geocodeLocation()
{
    const QString address = locationEdit_->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("请输入地址"),
                                 QStringLiteral("请先输入当前位置或地址"));
        return;
    }
    locationStatusLabel_->setText(QStringLiteral("正在由服务端解析地址…"));
    apiClient_.sendRequest(QStringLiteral("map.geocode"), {
        {QStringLiteral("address"), address}
    });
}

void MainWindow::setCurrentLocation(double latitude, double longitude,
                                    const QString &description, const QString &source)
{
    hasCurrentLocation_ = true;
    currentLatitude_ = latitude;
    currentLongitude_ = longitude;
    locationStatusLabel_->setText(QStringLiteral("%1：%2, %3（%4）")
        .arg(description)
        .arg(latitude, 0, 'f', 6)
        .arg(longitude, 0, 'f', 6)
        .arg(source));
    refreshStations();
}

void MainWindow::navigateSelectedStation(const QString &travelMode)
{
    if (!hasCurrentLocation_) {
        QMessageBox::information(this, QStringLiteral("缺少当前位置"),
                                 QStringLiteral("请先输入地址并解析当前位置"));
        return;
    }
    const int row = stationTable_->currentRow();
    if (row < 0 || !stationTable_->item(row, 0)) {
        QMessageBox::information(this, QStringLiteral("请选择站点"),
                                 QStringLiteral("请先选择一个充电站"));
        return;
    }
    const QTableWidgetItem *idItem = stationTable_->item(row, 0);
    const double stationLatitude = idItem->data(Qt::UserRole + 1).toDouble();
    const double stationLongitude = idItem->data(Qt::UserRole + 2).toDouble();
    const QString stationName = stationTable_->item(row, 1)->text();
    const QString mapKey = mapKey_;
    const QString mapReferer = mapReferer_;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("腾讯地图导航 · %1").arg(stationName));
    dialog.resize(980, 720);
    auto *layout = new QVBoxLayout(&dialog);
    auto *view = new QWebEngineView(&dialog);
    layout->addWidget(view);
    auto *modeBar = new QHBoxLayout;
    auto *drive = new QRadioButton(QStringLiteral("驾车"));
    auto *walk = new QRadioButton(QStringLiteral("步行"));
    auto *startNavigation = new QPushButton(QStringLiteral("开始导航"));
    (travelMode == QStringLiteral("walk") ? walk : drive)->setChecked(true);
    modeBar->addStretch();
    modeBar->addWidget(drive);
    modeBar->addWidget(walk);
    modeBar->addWidget(startNavigation);
    layout->addLayout(modeBar);

    auto routeUrl = [=] {
        const QString mode = walk->isChecked() ? QStringLiteral("walk") : QStringLiteral("drive");
        QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("type"), mode);
        query.addQueryItem(QStringLiteral("from"), QStringLiteral("当前位置"));
        query.addQueryItem(QStringLiteral("fromcoord"), QStringLiteral("%1,%2")
            .arg(currentLatitude_, 0, 'f', 6).arg(currentLongitude_, 0, 'f', 6));
        query.addQueryItem(QStringLiteral("to"), stationName);
        query.addQueryItem(QStringLiteral("tocoord"), QStringLiteral("%1,%2")
            .arg(stationLatitude, 0, 'f', 6).arg(stationLongitude, 0, 'f', 6));
        query.addQueryItem(QStringLiteral("policy"), QStringLiteral("0"));
        query.addQueryItem(QStringLiteral("referer"), mapReferer.isEmpty()
            ? QStringLiteral("EVCS-DEMO") : mapReferer);
        url.setQuery(query);
        return url;
    };
    connect(startNavigation, &QPushButton::clicked, &dialog, [view, routeUrl] {
        view->load(routeUrl());
    });
    if (mapKey.isEmpty()) {
        const double distance = straightLineDistanceKm(
            currentLatitude_, currentLongitude_, stationLatitude, stationLongitude);
        view->setHtml(QStringLiteral(
            "<html><meta charset='utf-8'><body style='font-family:sans-serif;padding:30px'>"
            "<h2>离线导航预览</h2><p>目的地：%1</p><p>直线距离：%2 km</p>"
            "<p style='color:#a55'>服务端未配置腾讯位置服务 Key。仍可用下方按钮尝试打开腾讯地图 URI 路线。</p>"
            "</body></html>")
            .arg(stationName.toHtmlEscaped()).arg(distance, 0, 'f', 2));
    } else {
        const QString html = QStringLiteral(
            "<!doctype html><html><head><meta charset='utf-8'><style>html,body,#map{height:100%;margin:0}</style>"
            "<script src='https://map.qq.com/api/gljs?v=1.exp&key=%1'></script></head>"
            "<body><div id='map'></div><script>"
            "const center=new TMap.LatLng(%2,%3);const destination=new TMap.LatLng(%4,%5);"
            "const map=new TMap.Map(document.getElementById('map'),{center:center,zoom:13});"
            "new TMap.MultiMarker({map:map,geometries:[{id:'from',position:center},{id:'to',position:destination}]});"
            "</script></body></html>")
            .arg(mapKey.toHtmlEscaped())
            .arg(currentLatitude_, 0, 'f', 6).arg(currentLongitude_, 0, 'f', 6)
            .arg(stationLatitude, 0, 'f', 6).arg(stationLongitude, 0, 'f', 6);
        view->setHtml(html, QUrl(QStringLiteral("https://map.qq.com/")));
    }
    dialog.exec();
}

void MainWindow::loadSelectedStation()
{
    const qint64 stationId = selectedId(stationTable_);
    if (stationId <= 0) return;
    stationViewStack_->setCurrentWidget(stationDetailView_);
    apiClient_.sendRequest(QStringLiteral("station.get"),
                           {{QStringLiteral("stationId"), static_cast<double>(stationId)}});
}

void MainWindow::reserveSelectedCharger()
{
    const qint64 chargerId = selectedId(chargerTable_);
    if (chargerId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("reservation.create"),
                           {{QStringLiteral("chargerId"), static_cast<double>(chargerId)}});
}

void MainWindow::refreshReservations()
{
    apiClient_.sendRequest(QStringLiteral("reservation.list"));
}

void MainWindow::cancelSelectedReservation()
{
    const qint64 reservationId = selectedId(reservationTable_);
    if (reservationId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("reservation.cancel"),
                           {{QStringLiteral("reservationId"), static_cast<double>(reservationId)}});
}

void MainWindow::startSelectedReservation()
{
    const qint64 reservationId = selectedId(reservationTable_);
    if (reservationId <= 0) return;
    const QTableWidgetItem *item = reservationTable_->item(reservationTable_->currentRow(), 0);
    const qint64 orderId = item ? item->data(Qt::UserRole + 1).toLongLong() : 0;
    if (orderId <= 0) {
        QMessageBox::warning(this, QStringLiteral("无法开始充电"),
                             QStringLiteral("当前预约没有对应的有效订单，请刷新后重试。"));
        return;
    }
    apiClient_.sendRequest(QStringLiteral("charging.start"),
                           {{QStringLiteral("orderId"), static_cast<double>(orderId)}});
}

void MainWindow::refreshChargingStatus()
{
    QJsonObject payload;
    if (activeSessionId_ > 0) {
        payload.insert(QStringLiteral("sessionId"), static_cast<double>(activeSessionId_));
    }
    apiClient_.sendRequest(QStringLiteral("charging.status"), payload);
}

void MainWindow::stopCharging()
{
    if (activeSessionId_ <= 0) return;
    apiClient_.sendRequest(QStringLiteral("charging.stop"),
                           {{QStringLiteral("sessionId"), static_cast<double>(activeSessionId_)}});
}

void MainWindow::refreshOrders()
{
    apiClient_.sendRequest(QStringLiteral("order.list"));
}

void MainWindow::showSelectedOrder()
{
    const qint64 orderId = selectedId(orderTable_);
    if (orderId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("order.get"),
                           {{QStringLiteral("orderId"), static_cast<double>(orderId)}});
}

void MainWindow::refreshProfile()
{
    apiClient_.sendRequest(QStringLiteral("user.profile"));
}

void MainWindow::editDisplayName()
{
    bool accepted = false;
    const QString value = QInputDialog::getText(
        this, QStringLiteral("修改昵称"), QStringLiteral("新昵称（1-20 位中文、英文、数字或下划线）"),
        QLineEdit::Normal, profileDisplayNameLabel_->text(), &accepted).trimmed();
    if (!accepted) return;
    static const QRegularExpression pattern(
        QStringLiteral("^[\\x{4e00}-\\x{9fff}A-Za-z0-9_]{1,20}$"));
    const QString lowered = value.toLower();
    const QStringList sensitiveWords{QStringLiteral("管理员"), QStringLiteral("系统"),
                                     QStringLiteral("客服"), QStringLiteral("admin"),
                                     QStringLiteral("root")};
    bool blocked = !pattern.match(value).hasMatch();
    for (const QString &word : sensitiveWords) blocked = blocked || lowered.contains(word);
    if (blocked) {
        QMessageBox::warning(this, QStringLiteral("昵称格式错误"),
                             QStringLiteral("昵称格式不正确或包含保留词"));
        return;
    }
    apiClient_.sendRequest(QStringLiteral("user.profile.update"), {
        {QStringLiteral("displayName"), value}
    });
}

void MainWindow::chooseAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("头像读取失败"),
                             QStringLiteral("无法读取所选图片"));
        return;
    }
    image = image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray encodedImage;
    QBuffer buffer(&encodedImage);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        QMessageBox::warning(this, QStringLiteral("头像处理失败"),
                             QStringLiteral("无法转换所选图片"));
        return;
    }
    apiClient_.sendRequest(QStringLiteral("user.avatar.update"), {
        {QStringLiteral("mimeType"), QStringLiteral("image/png")},
        {QStringLiteral("dataBase64"), QString::fromLatin1(encodedImage.toBase64())}
    });
}

void MainWindow::rechargeWallet()
{
    bool accepted = false;
    const double amount = QInputDialog::getDouble(
        this, QStringLiteral("钱包充值"), QStringLiteral("充值金额（教学模拟，不连接真实支付）"),
        100.0, 0.01, 9999.99, 2, &accepted);
    if (!accepted) return;
    apiClient_.sendRequest(QStringLiteral("wallet.recharge"), {
        {QStringLiteral("amountCents"), qRound64(amount * 100.0)}
    });
}

void MainWindow::logout()
{
    chargingTimer_.stop();
    apiClient_.sendRequest(QStringLiteral("auth.logout"));
}

qint64 MainWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0)) {
        QMessageBox::information(const_cast<MainWindow *>(this), QStringLiteral("请选择"),
                                 QStringLiteral("请先选择一行"));
        return 0;
    }
    return table->item(row, 0)->data(Qt::UserRole).toLongLong();
}


} // namespace evcs::userclient
