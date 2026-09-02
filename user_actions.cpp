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
            apiClient_.clearToken();
            stack_->setCurrentWidget(loginPage_);
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
        apiClient_.sendRequest(QStringLiteral("auth.phoneLogin"), {
            {QStringLiteral("phone"), usernameEdit_->text().trimmed()}
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

    struct OfflineLocation {
        const char *keyword;
        double latitude;
        double longitude;
        const char *description;
    };
    const OfflineLocation offlineLocations[] = {
        {"中关村", 39.9573, 116.3269, "北京市海淀区中关村"},
        {"海淀", 39.9573, 116.3269, "北京市海淀区（教学模拟位置）"},
        {"亦庄", 39.7942, 116.5068, "北京市大兴区亦庄"},
        {"大兴", 39.7942, 116.5068, "北京市大兴区（教学模拟位置）"},
        {"望京", 39.9979, 116.4878, "北京市朝阳区望京"},
        {"朝阳", 39.9979, 116.4878, "北京市朝阳区（教学模拟位置）"}
    };

    const QString mapKey = qEnvironmentVariable("EVCS_TENCENT_MAP_KEY").trimmed();
    if (mapKey.isEmpty()) {
        for (const OfflineLocation &candidate : offlineLocations) {
            if (address.contains(QString::fromUtf8(candidate.keyword), Qt::CaseInsensitive)) {
                setCurrentLocation(candidate.latitude, candidate.longitude,
                                   QString::fromUtf8(candidate.description),
                                   QStringLiteral("离线教学坐标"));
                return;
            }
        }
        QMessageBox::warning(
            this, QStringLiteral("无法解析地址"),
            QStringLiteral("当前未配置腾讯位置服务 Key。可输入中关村、海淀、亦庄、大兴、望京或朝阳使用离线教学坐标；联网解析请设置环境变量 EVCS_TENCENT_MAP_KEY。"));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address);
    query.addQueryItem(QStringLiteral("key"), mapKey);
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EVCS-Teaching-Platform/1.1"));
    QNetworkReply *reply = mapNetwork_->get(request);
    locationStatusLabel_->setText(QStringLiteral("正在调用腾讯地图解析…"));
    QTimer::singleShot(8000, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, address] {
        const QByteArray body = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            locationStatusLabel_->setText(QStringLiteral("腾讯地图解析失败"));
            QMessageBox::warning(this, QStringLiteral("地址解析失败"), networkMessage);
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject root = document.object();
        if (parseError.error != QJsonParseError::NoError
            || root.value(QStringLiteral("status")).toInt(-1) != 0) {
            const QString message = root.value(QStringLiteral("message")).toString(
                parseError.errorString());
            locationStatusLabel_->setText(QStringLiteral("腾讯地图解析失败"));
            QMessageBox::warning(this, QStringLiteral("地址解析失败"), message);
            return;
        }
        const QJsonObject result = root.value(QStringLiteral("result")).toObject();
        const QJsonObject location = result.value(QStringLiteral("location")).toObject();
        const double latitude = location.value(QStringLiteral("lat")).toDouble(999.0);
        const double longitude = location.value(QStringLiteral("lng")).toDouble(999.0);
        if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
            QMessageBox::warning(this, QStringLiteral("地址解析失败"),
                                 QStringLiteral("腾讯地图未返回有效坐标"));
            return;
        }
        const QString title = result.value(QStringLiteral("title")).toString(address);
        setCurrentLocation(latitude, longitude, title, QStringLiteral("腾讯地图 WebService"));
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
    const QString modeName = travelMode == QStringLiteral("walk")
        ? QStringLiteral("步行") : QStringLiteral("驾车");
    const QString mapKey = qEnvironmentVariable("EVCS_TENCENT_MAP_KEY").trimmed();
    const QString mapReferer = qEnvironmentVariable(
        "EVCS_TENCENT_MAP_REFERER", QStringLiteral("EVCS-DEMO")).trimmed();

    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), travelMode);
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

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("腾讯地图%1导航 · %2").arg(modeName, stationName));
    dialog.resize(980, 720);
    auto *layout = new QVBoxLayout(&dialog);
    auto *view = new QWebEngineView(&dialog);
    layout->addWidget(view);
    if (mapKey.isEmpty()) {
        const double distance = straightLineDistanceKm(
            currentLatitude_, currentLongitude_, stationLatitude, stationLongitude);
        view->setHtml(QStringLiteral(
            "<html><meta charset='utf-8'><body style='font-family:sans-serif;padding:30px'>"
            "<h2>离线导航预览</h2><p>方式：%1</p><p>目的地：%2</p>"
            "<p>直线距离：%3 km</p><p style='color:#a55'>未配置腾讯位置服务 Key，"
            "因此不请求在线路线；设置 EVCS_TENCENT_MAP_KEY 后可在本窗口加载腾讯地图路线。</p>"
            "<p><a href='%4'>尝试打开腾讯地图 URI</a></p></body></html>")
            .arg(modeName, stationName.toHtmlEscaped())
            .arg(distance, 0, 'f', 2)
            .arg(url.toString(QUrl::FullyEncoded)));
    } else {
        view->load(url);
    }
    dialog.exec();
}

void MainWindow::loadSelectedStation()
{
    const qint64 stationId = selectedId(stationTable_);
    if (stationId <= 0) return;
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

void MainWindow::startSelectedCharger()
{
    const qint64 chargerId = selectedId(chargerTable_);
    if (chargerId <= 0) return;
    apiClient_.sendRequest(QStringLiteral("charging.start"),
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
    apiClient_.sendRequest(QStringLiteral("charging.start"),
                           {{QStringLiteral("reservationId"), static_cast<double>(reservationId)}});
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
        this, QStringLiteral("修改昵称"), QStringLiteral("新昵称（1-32 个字符）"),
        QLineEdit::Normal, profileDisplayNameLabel_->text(), &accepted).trimmed();
    if (!accepted) return;
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
        100.0, 0.01, 10000.0, 2, &accepted);
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
