#include "businessservice.h"

#include "database.h"
#include "security.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtMath>

#include <algorithm>

namespace evcs::server {

namespace {

QString utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

double jsonId(qint64 value)
{
    return static_cast<double>(value);
}

qint64 jsonInteger(const QJsonValue &value)
{
    return static_cast<qint64>(value.toDouble());
}

ServiceResult databaseFailure(const QSqlQuery &query)
{
    return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                  query.lastError().text());
}

bool beginImmediate(const QSqlDatabase &database, QString *errorMessage)
{
    QSqlQuery query(database);
    if (query.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

QJsonObject stationFromQuery(const QSqlQuery &query)
{
    const int chargerCount = query.value(QStringLiteral("charger_count")).toInt();
    const int onlineCount = query.value(QStringLiteral("online_count")).toInt();
    return {
        {QStringLiteral("id"), jsonId(query.value(QStringLiteral("id")).toLongLong())},
        {QStringLiteral("name"), query.value(QStringLiteral("name")).toString()},
        {QStringLiteral("region"), query.value(QStringLiteral("region")).toString()},
        {QStringLiteral("address"), query.value(QStringLiteral("address")).toString()},
        {QStringLiteral("longitude"), query.value(QStringLiteral("longitude")).toDouble()},
        {QStringLiteral("latitude"), query.value(QStringLiteral("latitude")).toDouble()},
        {QStringLiteral("businessHours"), query.value(QStringLiteral("business_hours")).toString()},
        {QStringLiteral("status"), query.value(QStringLiteral("status")).toString()},
        {QStringLiteral("chargerCount"), chargerCount},
        {QStringLiteral("idleCount"), query.value(QStringLiteral("idle_count")).toInt()},
        {QStringLiteral("onlineCount"), onlineCount},
        {QStringLiteral("onlineRate"), chargerCount > 0
             ? static_cast<double>(onlineCount) / chargerCount : 0.0},
        {QStringLiteral("minimumPriceCentsPerKwh"),
         query.value(QStringLiteral("minimum_price")).toInt()}
    };
}

double distanceKm(double latitude1, double longitude1,
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

ServiceResult ServiceResult::success(const QJsonObject &data)
{
    return {true, data, {}, {}};
}

ServiceResult ServiceResult::failure(const QString &code, const QString &message)
{
    return {false, {}, code, message};
}

BusinessService::BusinessService(Database &database)
    : database_(database)
{
}

ServiceResult BusinessService::handle(const QString &action,
                                      const QJsonObject &payload,
                                      const QString &token)
{
    // 统一动作路由：网络层只传递协议对象，所有业务规则集中在服务层。
    if (action == QStringLiteral("auth.register")) return registerUser(payload);
    if (action == QStringLiteral("auth.login")) return login(payload);
    if (action == QStringLiteral("auth.phoneLogin")) return phoneLogin(payload);
    if (action == QStringLiteral("auth.logout")) return logout(token);
    if (action == QStringLiteral("user.profile")) return userProfile(token);
    if (action == QStringLiteral("user.profile.update")) return updateUserProfile(payload, token);
    if (action == QStringLiteral("user.avatar.update")) return updateUserAvatar(payload, token);
    if (action == QStringLiteral("wallet.recharge")) return rechargeWallet(payload, token);
    if (action == QStringLiteral("station.list")) return listStations(payload, token);
    if (action == QStringLiteral("station.get")) return getStation(payload, token);
    if (action == QStringLiteral("reservation.create")) return createReservation(payload, token);
    if (action == QStringLiteral("reservation.cancel")) return cancelReservation(payload, token);
    if (action == QStringLiteral("reservation.list")) return listReservations(token);
    if (action == QStringLiteral("charging.start")) return startCharging(payload, token);
    if (action == QStringLiteral("charging.status")) return chargingStatus(payload, token);
    if (action == QStringLiteral("charging.stop")) return stopCharging(payload, token);
    if (action == QStringLiteral("order.list")) return listOrders(token);
    if (action == QStringLiteral("order.get")) return getOrder(payload, token);
    if (action == QStringLiteral("admin.dashboard")) return adminDashboard(token);
    if (action == QStringLiteral("admin.analytics")) return adminAnalytics(token);
    if (action == QStringLiteral("admin.demo.generateHistory")) {
        return adminGenerateDemoHistory(payload, token);
    }
    if (action == QStringLiteral("admin.station.list")) return adminListStations(token);
    if (action == QStringLiteral("admin.station.save")) return adminSaveStation(payload, token);
    if (action == QStringLiteral("admin.charger.list")) return adminListChargers(payload, token);
    if (action == QStringLiteral("admin.charger.save")) return adminSaveCharger(payload, token);
    if (action == QStringLiteral("admin.charger.setStatus")) return adminSetChargerStatus(payload, token);
    if (action == QStringLiteral("admin.charger.restart")) return adminRestartCharger(payload, token);
    if (action == QStringLiteral("admin.charger.operation.list")) {
        return adminListChargerOperations(payload, token);
    }
    if (action == QStringLiteral("admin.user.list")) return adminListUsers(payload, token);
    if (action == QStringLiteral("admin.user.setStatus")) return adminSetUserStatus(payload, token);
    if (action == QStringLiteral("admin.order.list")) return adminListOrders(payload, token);
    if (action == QStringLiteral("admin.reservation.list")) return adminListReservations(token);
    if (action == QStringLiteral("admin.session.list")) return adminListChargingSessions(token);
    if (action == QStringLiteral("admin.tariff.list")) return adminListTariffs(token);
    if (action == QStringLiteral("admin.tariff.save")) return adminSaveTariff(payload, token);
    if (action == QStringLiteral("admin.fault.list")) return adminListFaults(token);
    if (action == QStringLiteral("admin.fault.save")) return adminSaveFault(payload, token);

    return ServiceResult::failure(QStringLiteral("FEATURE_NOT_READY"),
                                  QStringLiteral("该业务动作尚未实现"));
}

std::optional<BusinessService::UserContext> BusinessService::authenticate(
    const QString &token, ServiceResult *failureResult) const
{
    // 每次受保护请求都校验会话有效期和用户启用状态，避免只信任客户端缓存。
    if (token.trimmed().isEmpty()) {
        if (failureResult) {
            *failureResult = ServiceResult::failure(
                QStringLiteral("UNAUTHENTICATED"), QStringLiteral("请先登录"));
        }
        return std::nullopt;
    }

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT u.id, u.username, u.role "
        "FROM sessions s JOIN users u ON u.id = s.user_id "
        "WHERE s.token = ? AND s.expires_at > ? AND u.status = 'active'"));
    query.addBindValue(token);
    query.addBindValue(utcNow());
    if (!query.exec()) {
        if (failureResult) {
            *failureResult = databaseFailure(query);
        }
        return std::nullopt;
    }
    if (!query.next()) {
        if (failureResult) {
            *failureResult = ServiceResult::failure(
                QStringLiteral("UNAUTHENTICATED"), QStringLiteral("登录状态已失效"));
        }
        return std::nullopt;
    }
    return UserContext{
        query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toString()
    };
}

std::optional<BusinessService::UserContext> BusinessService::authenticateAdmin(
    const QString &token, ServiceResult *failureResult) const
{
    const auto user = authenticate(token, failureResult);
    if (!user) return std::nullopt;
    if (user->role != QStringLiteral("admin")) {
        if (failureResult) {
            *failureResult = ServiceResult::failure(
                QStringLiteral("FORBIDDEN"), QStringLiteral("需要管理员权限"));
        }
        return std::nullopt;
    }
    return user;
}

ServiceResult BusinessService::registerUser(const QJsonObject &payload)
{
    // 账号入口：校验用户输入、手机号唯一性，并只保存加盐后的密码摘要。
    const QString username = payload.value(QStringLiteral("username")).toString().trimmed();
    const QString password = payload.value(QStringLiteral("password")).toString();
    const QString displayName = payload.value(QStringLiteral("displayName")).toString().trimmed();
    const QString phone = payload.value(QStringLiteral("phone")).toString().trimmed();

    static const QRegularExpression usernamePattern(QStringLiteral("^[A-Za-z0-9_]{3,32}$"));
    if (!usernamePattern.match(username).hasMatch()) {
        return ServiceResult::failure(
            QStringLiteral("INVALID_ARGUMENT"),
            QStringLiteral("用户名必须由 3 到 32 位字母、数字或下划线组成"));
    }
    if (password.size() < 6 || password.size() > 128) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("密码长度必须为 6 到 128 位"));
    }
    static const QRegularExpression phonePattern(QStringLiteral("^1[3-9][0-9]{9}$"));
    if (!phone.isEmpty() && !phonePattern.match(phone).hasMatch()) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("手机号必须是合法的 11 位中国大陆手机号"));
    }

    QSqlQuery exists(database_.connection());
    exists.prepare(QStringLiteral("SELECT 1 FROM users WHERE username = ?"));
    exists.addBindValue(username);
    if (!exists.exec()) return databaseFailure(exists);
    if (exists.next()) {
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("用户名已存在"));
    }
    if (!phone.isEmpty()) {
        QSqlQuery phoneExists(database_.connection());
        phoneExists.prepare(QStringLiteral("SELECT 1 FROM users WHERE phone = ?"));
        phoneExists.addBindValue(phone);
        if (!phoneExists.exec()) return databaseFailure(phoneExists);
        if (phoneExists.next()) {
            return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                          QStringLiteral("手机号已被使用"));
        }
    }

    const QString salt = security::createSalt();
    const QString now = utcNow();
    QSqlQuery insert(database_.connection());
    insert.prepare(QStringLiteral(
        "INSERT INTO users(username, password_hash, password_salt, role, display_name, phone, "
        "balance_cents, status, created_at, updated_at) "
        "VALUES(?, ?, ?, 'user', ?, ?, 10000, 'active', ?, ?)"));
    insert.addBindValue(username);
    insert.addBindValue(security::hashPassword(password, salt));
    insert.addBindValue(salt);
    insert.addBindValue(displayName.isEmpty() ? username : displayName);
    insert.addBindValue(phone);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) return databaseFailure(insert);

    return ServiceResult::success({
        {QStringLiteral("userId"), jsonId(insert.lastInsertId().toLongLong())},
        {QStringLiteral("username"), username}
    });
}

ServiceResult BusinessService::login(const QJsonObject &payload)
{
    const QString username = payload.value(QStringLiteral("username")).toString().trimmed();
    const QString password = payload.value(QStringLiteral("password")).toString();
    if (username.isEmpty() || password.isEmpty()) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("请输入用户名和密码"));
    }

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT id, password_hash, password_salt, role, display_name, balance_cents, status "
        "FROM users WHERE username = ?"));
    query.addBindValue(username);
    if (!query.exec()) return databaseFailure(query);
    if (!query.next()
        || !security::verifyPassword(password, query.value(2).toString(), query.value(1).toString())) {
        return ServiceResult::failure(QStringLiteral("UNAUTHENTICATED"),
                                      QStringLiteral("用户名或密码错误"));
    }
    if (query.value(6).toString() != QStringLiteral("active")) {
        return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                      QStringLiteral("用户已被停用"));
    }

    const qint64 userId = query.value(0).toLongLong();
    const QString role = query.value(3).toString();
    const QString displayName = query.value(4).toString();
    const qint64 balanceCents = query.value(5).toLongLong();
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = utcNow();
    const QString expiresAt = QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODateWithMs);

    QSqlQuery cleanup(database_.connection());
    cleanup.prepare(QStringLiteral("DELETE FROM sessions WHERE expires_at <= ?"));
    cleanup.addBindValue(now);
    cleanup.exec();

    QSqlQuery insert(database_.connection());
    insert.prepare(QStringLiteral(
        "INSERT INTO sessions(token, user_id, expires_at, created_at) VALUES(?, ?, ?, ?)"));
    insert.addBindValue(token);
    insert.addBindValue(userId);
    insert.addBindValue(expiresAt);
    insert.addBindValue(now);
    if (!insert.exec()) return databaseFailure(insert);

    return ServiceResult::success({
        {QStringLiteral("token"), token},
        {QStringLiteral("expiresAt"), expiresAt},
        {QStringLiteral("user"), QJsonObject{
             {QStringLiteral("id"), jsonId(userId)},
             {QStringLiteral("username"), username},
             {QStringLiteral("displayName"), displayName},
             {QStringLiteral("role"), role},
             {QStringLiteral("balanceCents"), jsonId(balanceCents)}
         }}
    });
}

ServiceResult BusinessService::phoneLogin(const QJsonObject &payload)
{
    const QString phone = payload.value(QStringLiteral("phone")).toString().trimmed();
    static const QRegularExpression phonePattern(QStringLiteral("^1[3-9][0-9]{9}$"));
    if (!phonePattern.match(phone).hasMatch()) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("请输入合法的 11 位中国大陆手机号"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    auto rollback = [&database](const QSqlQuery &query) {
        const ServiceResult result = databaseFailure(query);
        database.rollback();
        return result;
    };

    qint64 userId = 0;
    QString username;
    QString displayName;
    qint64 balanceCents = 0;
    QString status;
    bool autoRegistered = false;

    QSqlQuery userQuery(database);
    userQuery.prepare(QStringLiteral(
        "SELECT id, username, display_name, balance_cents, status, role "
        "FROM users WHERE phone = ?"));
    userQuery.addBindValue(phone);
    if (!userQuery.exec()) return rollback(userQuery);
    if (userQuery.next()) {
        userId = userQuery.value(0).toLongLong();
        username = userQuery.value(1).toString();
        displayName = userQuery.value(2).toString();
        balanceCents = userQuery.value(3).toLongLong();
        status = userQuery.value(4).toString();
        if (userQuery.value(5).toString() != QStringLiteral("user")) {
            database.rollback();
            return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                          QStringLiteral("管理员请使用账号密码登录"));
        }
    } else {
        username = QStringLiteral("phone_%1").arg(phone);
        displayName = QStringLiteral("用户%1").arg(phone.right(4));
        balanceCents = 10000;
        status = QStringLiteral("active");
        const QString salt = security::createSalt();
        const QString generatedPassword = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString now = utcNow();
        QSqlQuery insert(database);
        insert.prepare(QStringLiteral(
            "INSERT INTO users(username, password_hash, password_salt, role, display_name, phone, "
            "balance_cents, status, created_at, updated_at) "
            "VALUES(?, ?, ?, 'user', ?, ?, ?, 'active', ?, ?)"));
        insert.addBindValue(username);
        insert.addBindValue(security::hashPassword(generatedPassword, salt));
        insert.addBindValue(salt);
        insert.addBindValue(displayName);
        insert.addBindValue(phone);
        insert.addBindValue(balanceCents);
        insert.addBindValue(now);
        insert.addBindValue(now);
        if (!insert.exec()) return rollback(insert);
        userId = insert.lastInsertId().toLongLong();
        autoRegistered = true;
    }

    if (status != QStringLiteral("active")) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                      QStringLiteral("用户已被停用"));
    }

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = utcNow();
    const QString expiresAt = QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODateWithMs);
    QSqlQuery cleanup(database);
    cleanup.prepare(QStringLiteral("DELETE FROM sessions WHERE expires_at <= ?"));
    cleanup.addBindValue(now);
    if (!cleanup.exec()) return rollback(cleanup);

    QSqlQuery session(database);
    session.prepare(QStringLiteral(
        "INSERT INTO sessions(token, user_id, expires_at, created_at) VALUES(?, ?, ?, ?)"));
    session.addBindValue(token);
    session.addBindValue(userId);
    session.addBindValue(expiresAt);
    session.addBindValue(now);
    if (!session.exec()) return rollback(session);
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), database.lastError().text());
    }

    return ServiceResult::success({
        {QStringLiteral("token"), token},
        {QStringLiteral("expiresAt"), expiresAt},
        {QStringLiteral("autoRegistered"), autoRegistered},
        {QStringLiteral("user"), QJsonObject{
             {QStringLiteral("id"), jsonId(userId)},
             {QStringLiteral("username"), username},
             {QStringLiteral("displayName"), displayName},
             {QStringLiteral("phone"), phone},
             {QStringLiteral("role"), QStringLiteral("user")},
             {QStringLiteral("balanceCents"), jsonId(balanceCents)}
         }}
    });
}

ServiceResult BusinessService::logout(const QString &token)
{
    ServiceResult failure;
    if (!authenticate(token, &failure)) return failure;

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral("DELETE FROM sessions WHERE token = ?"));
    query.addBindValue(token);
    if (!query.exec()) return databaseFailure(query);
    return ServiceResult::success();
}

ServiceResult BusinessService::userProfile(const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT id, username, display_name, phone, role, balance_cents, status, created_at, "
        "avatar_mime, avatar_data "
        "FROM users WHERE id = ?"));
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);
    if (!query.next()) {
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("用户不存在"));
    }
    QJsonObject userObject{
             {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
             {QStringLiteral("username"), query.value(1).toString()},
             {QStringLiteral("displayName"), query.value(2).toString()},
             {QStringLiteral("phone"), query.value(3).toString()},
             {QStringLiteral("role"), query.value(4).toString()},
             {QStringLiteral("balanceCents"), jsonId(query.value(5).toLongLong())},
             {QStringLiteral("status"), query.value(6).toString()},
             {QStringLiteral("createdAt"), query.value(7).toString()},
             {QStringLiteral("avatarMime"), query.value(8).toString()}
    };
    const QByteArray avatarData = query.value(9).toByteArray();
    if (!avatarData.isEmpty()) {
        userObject.insert(QStringLiteral("avatarBase64"),
                          QString::fromLatin1(avatarData.toBase64()));
    }
    return ServiceResult::success({{QStringLiteral("user"), userObject}});
}

ServiceResult BusinessService::updateUserProfile(const QJsonObject &payload,
                                                 const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    const QString displayName = payload.value(QStringLiteral("displayName")).toString().trimmed();
    if (displayName.isEmpty() || displayName.size() > 32) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("昵称长度必须为 1 到 32 个字符"));
    }
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "UPDATE users SET display_name = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(displayName);
    query.addBindValue(utcNow());
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);
    return ServiceResult::success({{QStringLiteral("displayName"), displayName}});
}

ServiceResult BusinessService::updateUserAvatar(const QJsonObject &payload,
                                                const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    const QString mime = payload.value(QStringLiteral("mimeType")).toString().trimmed().toLower();
    const QString encoded = payload.value(QStringLiteral("dataBase64")).toString();
    if (!QStringList{QStringLiteral("image/png"), QStringLiteral("image/jpeg")}.contains(mime)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("头像仅支持 PNG 或 JPEG"));
    }
    const QByteArray data = QByteArray::fromBase64(encoded.toLatin1());
    if (data.isEmpty() || data.size() > 512 * 1024) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("头像不能为空且不得超过 512 KiB"));
    }
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "UPDATE users SET avatar_mime = ?, avatar_data = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(mime);
    query.addBindValue(data);
    query.addBindValue(utcNow());
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);
    return ServiceResult::success({{QStringLiteral("avatarBytes"), data.size()}});
}

ServiceResult BusinessService::rechargeWallet(const QJsonObject &payload,
                                              const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    if (user->role != QStringLiteral("user")) {
        return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                      QStringLiteral("仅普通用户可以充值"));
    }
    const qint64 amountCents = jsonInteger(payload.value(QStringLiteral("amountCents")));
    if (amountCents < 1 || amountCents > 1000000) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("单次充值金额必须在 0.01 到 10000 元之间"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE users SET balance_cents = balance_cents + ?, updated_at = ? WHERE id = ?"));
    const QString now = utcNow();
    update.addBindValue(amountCents);
    update.addBindValue(now);
    update.addBindValue(user->id);
    if (!update.exec()) {
        const ServiceResult result = databaseFailure(update);
        database.rollback();
        return result;
    }
    QSqlQuery balance(database);
    balance.prepare(QStringLiteral("SELECT balance_cents FROM users WHERE id = ?"));
    balance.addBindValue(user->id);
    if (!balance.exec() || !balance.next()) {
        const ServiceResult result = databaseFailure(balance);
        database.rollback();
        return result;
    }
    const qint64 balanceAfter = balance.value(0).toLongLong();
    QSqlQuery record(database);
    record.prepare(QStringLiteral(
        "INSERT INTO recharge_records(user_id, amount_cents, balance_after_cents, channel, created_at) "
        "VALUES(?, ?, ?, 'demo', ?)"));
    record.addBindValue(user->id);
    record.addBindValue(amountCents);
    record.addBindValue(balanceAfter);
    record.addBindValue(now);
    if (!record.exec()) {
        const ServiceResult result = databaseFailure(record);
        database.rollback();
        return result;
    }
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), database.lastError().text());
    }
    return ServiceResult::success({
        {QStringLiteral("rechargeId"), jsonId(record.lastInsertId().toLongLong())},
        {QStringLiteral("amountCents"), jsonId(amountCents)},
        {QStringLiteral("balanceCents"), jsonId(balanceAfter)},
        {QStringLiteral("channel"), QStringLiteral("demo")}
    });
}

ServiceResult BusinessService::listStations(const QJsonObject &payload, const QString &token)
{
    // 站点查询：汇总空闲桩、在线率和最低价，并按用户坐标计算距离。
    ServiceResult failure;
    if (!authenticate(token, &failure)) return failure;

    const QString keyword = payload.value(QStringLiteral("keyword")).toString().trimmed();
    const QString region = payload.value(QStringLiteral("region")).toString().trimmed();
    const bool onlyAvailable = payload.value(QStringLiteral("onlyAvailable")).toBool(false);

    QString sql = QStringLiteral(
        "SELECT s.id, s.name, s.region, s.address, s.longitude, s.latitude, "
        "s.business_hours, s.status, COUNT(c.id) AS charger_count, "
        "COALESCE(SUM(CASE WHEN c.status = 'idle' THEN 1 ELSE 0 END), 0) AS idle_count, "
        "COALESCE(SUM(CASE WHEN c.status NOT IN ('offline', 'disabled') THEN 1 ELSE 0 END), 0) "
        "AS online_count, "
        "COALESCE(MIN(t.price_cents_per_kwh), 0) AS minimum_price "
        "FROM stations s "
        "LEFT JOIN chargers c ON c.station_id = s.id "
        "LEFT JOIN tariffs t ON t.id = c.tariff_id "
        "WHERE s.status = 'active' ");
    if (!keyword.isEmpty()) {
        sql += QStringLiteral("AND (s.name LIKE ? OR s.address LIKE ?) ");
    }
    if (!region.isEmpty()) {
        sql += QStringLiteral("AND s.region = ? ");
    }
    sql += QStringLiteral("GROUP BY s.id ");
    if (onlyAvailable) {
        sql += QStringLiteral("HAVING idle_count > 0 ");
    }
    sql += QStringLiteral("ORDER BY idle_count DESC, s.id ASC");

    QSqlQuery query(database_.connection());
    query.prepare(sql);
    if (!keyword.isEmpty()) {
        query.addBindValue(QStringLiteral("%%1%").arg(keyword));
        query.addBindValue(QStringLiteral("%%1%").arg(keyword));
    }
    if (!region.isEmpty()) query.addBindValue(region);
    if (!query.exec()) return databaseFailure(query);

    QList<QJsonObject> stationList;
    const bool hasOrigin = payload.contains(QStringLiteral("longitude"))
        && payload.contains(QStringLiteral("latitude"))
        && payload.value(QStringLiteral("longitude")).isDouble()
        && payload.value(QStringLiteral("latitude")).isDouble();
    const double originLongitude = payload.value(QStringLiteral("longitude")).toDouble();
    const double originLatitude = payload.value(QStringLiteral("latitude")).toDouble();
    while (query.next()) {
        QJsonObject station = stationFromQuery(query);
        if (hasOrigin && originLongitude >= -180.0 && originLongitude <= 180.0
            && originLatitude >= -90.0 && originLatitude <= 90.0) {
            station.insert(QStringLiteral("distanceKm"),
                           distanceKm(originLatitude, originLongitude,
                                      station.value(QStringLiteral("latitude")).toDouble(),
                                      station.value(QStringLiteral("longitude")).toDouble()));
        }
        stationList.append(station);
    }
    if (hasOrigin) {
        std::sort(stationList.begin(), stationList.end(), [](const QJsonObject &left,
                                                             const QJsonObject &right) {
            return left.value(QStringLiteral("distanceKm")).toDouble()
                < right.value(QStringLiteral("distanceKm")).toDouble();
        });
    }
    QJsonArray stations;
    for (const QJsonObject &station : stationList) stations.append(station);
    return ServiceResult::success({{QStringLiteral("stations"), stations}});
}

ServiceResult BusinessService::getStation(const QJsonObject &payload, const QString &token)
{
    ServiceResult failure;
    if (!authenticate(token, &failure)) return failure;

    const qint64 stationId = static_cast<qint64>(payload.value(QStringLiteral("stationId")).toDouble());
    if (stationId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("stationId 无效"));
    }

    QSqlQuery station(database_.connection());
    station.prepare(QStringLiteral(
        "SELECT s.id, s.name, s.region, s.address, s.longitude, s.latitude, "
        "s.business_hours, s.status, COUNT(c.id) AS charger_count, "
        "COALESCE(SUM(CASE WHEN c.status = 'idle' THEN 1 ELSE 0 END), 0) AS idle_count, "
        "COALESCE(SUM(CASE WHEN c.status NOT IN ('offline', 'disabled') THEN 1 ELSE 0 END), 0) "
        "AS online_count, "
        "COALESCE(MIN(t.price_cents_per_kwh), 0) AS minimum_price "
        "FROM stations s LEFT JOIN chargers c ON c.station_id = s.id "
        "LEFT JOIN tariffs t ON t.id = c.tariff_id WHERE s.id = ? GROUP BY s.id"));
    station.addBindValue(stationId);
    if (!station.exec()) return databaseFailure(station);
    if (!station.next()) {
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("充电站不存在"));
    }

    QJsonObject result = stationFromQuery(station);
    QSqlQuery chargers(database_.connection());
    chargers.prepare(QStringLiteral(
        "SELECT c.id, c.code, c.connector_type, c.rated_power_kw, c.status, "
        "t.id AS tariff_id, t.name AS tariff_name, t.price_cents_per_kwh "
        "FROM chargers c JOIN tariffs t ON t.id = c.tariff_id "
        "WHERE c.station_id = ? ORDER BY c.id"));
    chargers.addBindValue(stationId);
    if (!chargers.exec()) return databaseFailure(chargers);

    QJsonArray chargerArray;
    while (chargers.next()) {
        chargerArray.append(QJsonObject{
            {QStringLiteral("id"), jsonId(chargers.value(0).toLongLong())},
            {QStringLiteral("code"), chargers.value(1).toString()},
            {QStringLiteral("connectorType"), chargers.value(2).toString()},
            {QStringLiteral("ratedPowerKw"), chargers.value(3).toDouble()},
            {QStringLiteral("status"), chargers.value(4).toString()},
            {QStringLiteral("tariffId"), jsonId(chargers.value(5).toLongLong())},
            {QStringLiteral("tariffName"), chargers.value(6).toString()},
            {QStringLiteral("priceCentsPerKwh"), chargers.value(7).toInt()}
        });
    }
    result.insert(QStringLiteral("chargers"), chargerArray);
    return ServiceResult::success({{QStringLiteral("station"), result}});
}

bool BusinessService::expireReservations(QString *errorMessage) const
{
    // 预约过期时同时释放充电桩，保证预约表与设备状态始终一致。
    const QString now = utcNow();
    QSqlQuery select(database_.connection());
    select.prepare(QStringLiteral(
        "SELECT charger_id FROM reservations WHERE status = 'active' AND expires_at <= ?"));
    select.addBindValue(now);
    if (!select.exec()) {
        if (errorMessage) *errorMessage = select.lastError().text();
        return false;
    }

    QList<qint64> chargerIds;
    while (select.next()) chargerIds.append(select.value(0).toLongLong());

    QSqlQuery expire(database_.connection());
    expire.prepare(QStringLiteral(
        "UPDATE reservations SET status = 'expired', completed_at = ? "
        "WHERE status = 'active' AND expires_at <= ?"));
    expire.addBindValue(now);
    expire.addBindValue(now);
    if (!expire.exec()) {
        if (errorMessage) *errorMessage = expire.lastError().text();
        return false;
    }

    for (qint64 chargerId : chargerIds) {
        QSqlQuery release(database_.connection());
        release.prepare(QStringLiteral(
            "UPDATE chargers SET status = 'idle', updated_at = ? "
            "WHERE id = ? AND status = 'reserved'"));
        release.addBindValue(now);
        release.addBindValue(chargerId);
        if (!release.exec()) {
            if (errorMessage) *errorMessage = release.lastError().text();
            return false;
        }
    }
    return true;
}

ServiceResult BusinessService::createReservation(const QJsonObject &payload,
                                                  const QString &token)
{
    // 预约采用立即事务锁定，防止多个客户端同时抢占同一充电桩。
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    if (user->role != QStringLiteral("user")) {
        return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                      QStringLiteral("管理员不能创建充电预约"));
    }

    const qint64 chargerId = static_cast<qint64>(
        payload.value(QStringLiteral("chargerId")).toDouble());
    if (chargerId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("chargerId 无效"));
    }

    QSqlDatabase database = database_.connection();
    QString transactionError;
    if (!beginImmediate(database, &transactionError)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), transactionError);
    }
    if (!expireReservations(&transactionError)) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), transactionError);
    }

    QSqlQuery active(database);
    active.prepare(QStringLiteral(
        "SELECT 1 FROM reservations WHERE user_id = ? AND status = 'active' "
        "UNION ALL SELECT 1 FROM charging_sessions WHERE user_id = ? AND status = 'charging' "
        "LIMIT 1"));
    active.addBindValue(user->id);
    active.addBindValue(user->id);
    if (!active.exec()) {
        const ServiceResult result = databaseFailure(active);
        database.rollback();
        return result;
    }
    if (active.next()) {
        database.rollback();
        return ServiceResult::failure(
            QStringLiteral("CONFLICT"),
            QStringLiteral("当前已有有效预约或进行中的充电"));
    }

    QSqlQuery charger(database);
    charger.prepare(QStringLiteral(
        "SELECT c.status, s.status FROM chargers c "
        "JOIN stations s ON s.id = c.station_id WHERE c.id = ?"));
    charger.addBindValue(chargerId);
    if (!charger.exec()) {
        const ServiceResult result = databaseFailure(charger);
        database.rollback();
        return result;
    }
    if (!charger.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("充电桩不存在"));
    }
    if (charger.value(0).toString() != QStringLiteral("idle")
        || charger.value(1).toString() != QStringLiteral("active")) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CHARGER_UNAVAILABLE"),
                                      QStringLiteral("充电桩当前不可预约"));
    }

    const QDateTime reservedAt = QDateTime::currentDateTimeUtc();
    const QString reservedAtText = reservedAt.toString(Qt::ISODateWithMs);
    const QString expiresAt = reservedAt.addSecs(15 * 60).toString(Qt::ISODateWithMs);
    QSqlQuery reserveCharger(database);
    reserveCharger.prepare(QStringLiteral(
        "UPDATE chargers SET status = 'reserved', updated_at = ? "
        "WHERE id = ? AND status = 'idle'"));
    reserveCharger.addBindValue(reservedAtText);
    reserveCharger.addBindValue(chargerId);
    if (!reserveCharger.exec() || reserveCharger.numRowsAffected() != 1) {
        const QString message = reserveCharger.lastError().isValid()
            ? reserveCharger.lastError().text()
            : QStringLiteral("充电桩已被其他用户占用");
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CHARGER_UNAVAILABLE"), message);
    }

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO reservations(user_id, charger_id, status, reserved_at, expires_at) "
        "VALUES(?, ?, 'active', ?, ?)"));
    insert.addBindValue(user->id);
    insert.addBindValue(chargerId);
    insert.addBindValue(reservedAtText);
    insert.addBindValue(expiresAt);
    if (!insert.exec()) {
        const ServiceResult result = databaseFailure(insert);
        database.rollback();
        return result;
    }
    const qint64 reservationId = insert.lastInsertId().toLongLong();
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                      database.lastError().text());
    }

    return ServiceResult::success({
        {QStringLiteral("reservationId"), jsonId(reservationId)},
        {QStringLiteral("chargerId"), jsonId(chargerId)},
        {QStringLiteral("reservedAt"), reservedAtText},
        {QStringLiteral("expiresAt"), expiresAt},
        {QStringLiteral("status"), QStringLiteral("active")}
    });
}

ServiceResult BusinessService::cancelReservation(const QJsonObject &payload,
                                                 const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    const qint64 reservationId = static_cast<qint64>(
        payload.value(QStringLiteral("reservationId")).toDouble());
    if (reservationId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("reservationId 无效"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    if (!expireReservations(&error)) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }

    QSqlQuery select(database);
    select.prepare(QStringLiteral(
        "SELECT charger_id, status FROM reservations WHERE id = ? AND user_id = ?"));
    select.addBindValue(reservationId);
    select.addBindValue(user->id);
    if (!select.exec()) {
        const ServiceResult result = databaseFailure(select);
        database.rollback();
        return result;
    }
    if (!select.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("预约不存在"));
    }
    if (select.value(1).toString() != QStringLiteral("active")) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("该预约不能取消"));
    }
    const qint64 chargerId = select.value(0).toLongLong();
    const QString now = utcNow();

    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE reservations SET status = 'cancelled', completed_at = ? WHERE id = ?"));
    update.addBindValue(now);
    update.addBindValue(reservationId);
    if (!update.exec()) {
        const ServiceResult result = databaseFailure(update);
        database.rollback();
        return result;
    }
    QSqlQuery release(database);
    release.prepare(QStringLiteral(
        "UPDATE chargers SET status = 'idle', updated_at = ? "
        "WHERE id = ? AND status = 'reserved'"));
    release.addBindValue(now);
    release.addBindValue(chargerId);
    if (!release.exec()) {
        const ServiceResult result = databaseFailure(release);
        database.rollback();
        return result;
    }
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                      database.lastError().text());
    }
    return ServiceResult::success({{QStringLiteral("reservationId"), jsonId(reservationId)}});
}

ServiceResult BusinessService::listReservations(const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;

    QString error;
    if (!expireReservations(&error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT r.id, r.status, r.reserved_at, r.expires_at, r.completed_at, "
        "c.id, c.code, s.id, s.name, s.address "
        "FROM reservations r JOIN chargers c ON c.id = r.charger_id "
        "JOIN stations s ON s.id = c.station_id "
        "WHERE r.user_id = ? ORDER BY r.id DESC LIMIT 100"));
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);

    QJsonArray reservations;
    while (query.next()) {
        reservations.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("status"), query.value(1).toString()},
            {QStringLiteral("reservedAt"), query.value(2).toString()},
            {QStringLiteral("expiresAt"), query.value(3).toString()},
            {QStringLiteral("completedAt"), query.value(4).toString()},
            {QStringLiteral("chargerId"), jsonId(query.value(5).toLongLong())},
            {QStringLiteral("chargerCode"), query.value(6).toString()},
            {QStringLiteral("stationId"), jsonId(query.value(7).toLongLong())},
            {QStringLiteral("stationName"), query.value(8).toString()},
            {QStringLiteral("stationAddress"), query.value(9).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("reservations"), reservations}});
}

ServiceResult BusinessService::startCharging(const QJsonObject &payload,
                                              const QString &token)
{
    // 启动充电前在同一事务中核对用户、预约、设备和钱包状态。
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    if (user->role != QStringLiteral("user")) {
        return ServiceResult::failure(QStringLiteral("FORBIDDEN"),
                                      QStringLiteral("管理员不能开始充电"));
    }

    qint64 chargerId = static_cast<qint64>(payload.value(QStringLiteral("chargerId")).toDouble());
    const qint64 reservationId = static_cast<qint64>(
        payload.value(QStringLiteral("reservationId")).toDouble());
    if (chargerId <= 0 && reservationId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("需要 chargerId 或 reservationId"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    if (!expireReservations(&error)) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }

    QSqlQuery active(database);
    active.prepare(QStringLiteral(
        "SELECT 1 FROM charging_sessions WHERE user_id = ? AND status = 'charging'"));
    active.addBindValue(user->id);
    if (!active.exec()) {
        const ServiceResult result = databaseFailure(active);
        database.rollback();
        return result;
    }
    if (active.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("已有进行中的充电"));
    }

    if (reservationId > 0) {
        QSqlQuery reservation(database);
        reservation.prepare(QStringLiteral(
            "SELECT charger_id FROM reservations "
            "WHERE id = ? AND user_id = ? AND status = 'active' AND expires_at > ?"));
        reservation.addBindValue(reservationId);
        reservation.addBindValue(user->id);
        reservation.addBindValue(utcNow());
        if (!reservation.exec()) {
            const ServiceResult result = databaseFailure(reservation);
            database.rollback();
            return result;
        }
        if (!reservation.next()) {
            database.rollback();
            return ServiceResult::failure(QStringLiteral("RESERVATION_EXPIRED"),
                                          QStringLiteral("预约无效或已过期"));
        }
        chargerId = reservation.value(0).toLongLong();
    } else {
        QSqlQuery activeReservation(database);
        activeReservation.prepare(QStringLiteral(
            "SELECT 1 FROM reservations WHERE user_id = ? AND status = 'active'"));
        activeReservation.addBindValue(user->id);
        if (!activeReservation.exec()) {
            const ServiceResult result = databaseFailure(activeReservation);
            database.rollback();
            return result;
        }
        if (activeReservation.next()) {
            database.rollback();
            return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                          QStringLiteral("请先使用或取消当前预约"));
        }
    }

    QSqlQuery charger(database);
    charger.prepare(QStringLiteral(
        "SELECT c.status, c.rated_power_kw, t.price_cents_per_kwh, s.status "
        "FROM chargers c JOIN tariffs t ON t.id = c.tariff_id "
        "JOIN stations s ON s.id = c.station_id WHERE c.id = ?"));
    charger.addBindValue(chargerId);
    if (!charger.exec()) {
        const ServiceResult result = databaseFailure(charger);
        database.rollback();
        return result;
    }
    if (!charger.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("充电桩不存在"));
    }
    const QString expectedStatus = reservationId > 0
        ? QStringLiteral("reserved") : QStringLiteral("idle");
    if (charger.value(0).toString() != expectedStatus
        || charger.value(3).toString() != QStringLiteral("active")) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CHARGER_UNAVAILABLE"),
                                      QStringLiteral("充电桩当前不可用"));
    }

    const double powerKw = charger.value(1).toDouble();
    const int priceCentsPerKwh = charger.value(2).toInt();
    const QString now = utcNow();
    if (reservationId > 0) {
        QSqlQuery useReservation(database);
        useReservation.prepare(QStringLiteral(
            "UPDATE reservations SET status = 'used', completed_at = ? WHERE id = ?"));
        useReservation.addBindValue(now);
        useReservation.addBindValue(reservationId);
        if (!useReservation.exec()) {
            const ServiceResult result = databaseFailure(useReservation);
            database.rollback();
            return result;
        }
    }

    QSqlQuery occupy(database);
    occupy.prepare(QStringLiteral(
        "UPDATE chargers SET status = 'charging', updated_at = ? WHERE id = ? AND status = ?"));
    occupy.addBindValue(now);
    occupy.addBindValue(chargerId);
    occupy.addBindValue(expectedStatus);
    if (!occupy.exec() || occupy.numRowsAffected() != 1) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CHARGER_UNAVAILABLE"),
                                      QStringLiteral("充电桩状态已变化"));
    }

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO charging_sessions(user_id, charger_id, reservation_id, status, started_at, "
        "energy_wh, price_cents_per_kwh, amount_cents) "
        "VALUES(?, ?, ?, 'charging', ?, 0, ?, 0)"));
    insert.addBindValue(user->id);
    insert.addBindValue(chargerId);
    if (reservationId > 0) insert.addBindValue(reservationId);
    else insert.addBindValue(QVariant{});
    insert.addBindValue(now);
    insert.addBindValue(priceCentsPerKwh);
    if (!insert.exec()) {
        const ServiceResult result = databaseFailure(insert);
        database.rollback();
        return result;
    }
    const qint64 sessionId = insert.lastInsertId().toLongLong();
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                      database.lastError().text());
    }

    return ServiceResult::success({
        {QStringLiteral("sessionId"), jsonId(sessionId)},
        {QStringLiteral("chargerId"), jsonId(chargerId)},
        {QStringLiteral("status"), QStringLiteral("charging")},
        {QStringLiteral("startedAt"), now},
        {QStringLiteral("ratedPowerKw"), powerKw},
        {QStringLiteral("priceCentsPerKwh"), priceCentsPerKwh}
    });
}

ServiceResult BusinessService::chargingStatus(const QJsonObject &payload,
                                              const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;

    const qint64 sessionId = static_cast<qint64>(
        payload.value(QStringLiteral("sessionId")).toDouble());
    QString sql = QStringLiteral(
        "SELECT cs.id, cs.charger_id, cs.status, cs.started_at, cs.ended_at, "
        "cs.energy_wh, cs.price_cents_per_kwh, cs.amount_cents, c.rated_power_kw, c.code, s.name "
        "FROM charging_sessions cs JOIN chargers c ON c.id = cs.charger_id "
        "JOIN stations s ON s.id = c.station_id WHERE cs.user_id = ? ");
    sql += sessionId > 0 ? QStringLiteral("AND cs.id = ?")
                         : QStringLiteral("AND cs.status = 'charging' ORDER BY cs.id DESC LIMIT 1");

    QSqlQuery query(database_.connection());
    query.prepare(sql);
    query.addBindValue(user->id);
    if (sessionId > 0) query.addBindValue(sessionId);
    if (!query.exec()) return databaseFailure(query);
    if (!query.next()) {
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("未找到充电会话"));
    }

    const QString status = query.value(2).toString();
    qint64 energyWh = query.value(5).toLongLong();
    qint64 amountCents = query.value(7).toLongLong();
    qint64 elapsedSeconds = 0;
    if (status == QStringLiteral("charging")) {
        const QDateTime startedAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs);
        elapsedSeconds = qMax<qint64>(0, startedAt.secsTo(QDateTime::currentDateTimeUtc()));
        energyWh = qRound64(query.value(8).toDouble() * 1000.0
                            * static_cast<double>(elapsedSeconds) / 3600.0);
        amountCents = qRound64(static_cast<double>(energyWh)
                               * query.value(6).toInt() / 1000.0);
    } else {
        const QDateTime startedAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs);
        const QDateTime endedAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODateWithMs);
        elapsedSeconds = qMax<qint64>(0, startedAt.secsTo(endedAt));
    }

    return ServiceResult::success({
        {QStringLiteral("session"), QJsonObject{
             {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
             {QStringLiteral("chargerId"), jsonId(query.value(1).toLongLong())},
             {QStringLiteral("chargerCode"), query.value(9).toString()},
             {QStringLiteral("stationName"), query.value(10).toString()},
             {QStringLiteral("status"), status},
             {QStringLiteral("startedAt"), query.value(3).toString()},
             {QStringLiteral("endedAt"), query.value(4).toString()},
             {QStringLiteral("elapsedSeconds"), jsonId(elapsedSeconds)},
             {QStringLiteral("energyWh"), jsonId(energyWh)},
             {QStringLiteral("amountCents"), jsonId(amountCents)},
             {QStringLiteral("priceCentsPerKwh"), query.value(6).toInt()}
         }}
    });
}

ServiceResult BusinessService::stopCharging(const QJsonObject &payload,
                                             const QString &token)
{
    // 停止充电时结算电量与金额，并同步生成订单、扣款和释放设备。
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    const qint64 requestedSessionId = static_cast<qint64>(
        payload.value(QStringLiteral("sessionId")).toDouble());

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }

    QString sql = QStringLiteral(
        "SELECT cs.id, cs.charger_id, cs.started_at, cs.price_cents_per_kwh, "
        "c.rated_power_kw, c.station_id FROM charging_sessions cs "
        "JOIN chargers c ON c.id = cs.charger_id "
        "WHERE cs.user_id = ? AND cs.status = 'charging' ");
    sql += requestedSessionId > 0 ? QStringLiteral("AND cs.id = ?")
                                  : QStringLiteral("ORDER BY cs.id DESC LIMIT 1");
    QSqlQuery select(database);
    select.prepare(sql);
    select.addBindValue(user->id);
    if (requestedSessionId > 0) select.addBindValue(requestedSessionId);
    if (!select.exec()) {
        const ServiceResult result = databaseFailure(select);
        database.rollback();
        return result;
    }
    if (!select.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("没有进行中的充电"));
    }

    const qint64 sessionId = select.value(0).toLongLong();
    const qint64 chargerId = select.value(1).toLongLong();
    const QDateTime startedAt = QDateTime::fromString(select.value(2).toString(), Qt::ISODateWithMs);
    const int priceCentsPerKwh = select.value(3).toInt();
    const double powerKw = select.value(4).toDouble();
    const qint64 stationId = select.value(5).toLongLong();
    const QDateTime endedAt = QDateTime::currentDateTimeUtc();
    const qint64 elapsedSeconds = qMax<qint64>(0, startedAt.secsTo(endedAt));
    const qint64 energyWh = qRound64(powerKw * 1000.0
                                     * static_cast<double>(elapsedSeconds) / 3600.0);
    const qint64 amountCents = qRound64(static_cast<double>(energyWh)
                                        * priceCentsPerKwh / 1000.0);
    const QString endedAtText = endedAt.toString(Qt::ISODateWithMs);

    QSqlQuery updateSession(database);
    updateSession.prepare(QStringLiteral(
        "UPDATE charging_sessions SET status = 'finished', ended_at = ?, energy_wh = ?, "
        "amount_cents = ? WHERE id = ? AND status = 'charging'"));
    updateSession.addBindValue(endedAtText);
    updateSession.addBindValue(energyWh);
    updateSession.addBindValue(amountCents);
    updateSession.addBindValue(sessionId);
    if (!updateSession.exec()) {
        const ServiceResult result = databaseFailure(updateSession);
        database.rollback();
        return result;
    }

    QSqlQuery release(database);
    release.prepare(QStringLiteral(
        "UPDATE chargers SET status = 'idle', updated_at = ? WHERE id = ?"));
    release.addBindValue(endedAtText);
    release.addBindValue(chargerId);
    if (!release.exec()) {
        const ServiceResult result = databaseFailure(release);
        database.rollback();
        return result;
    }

    QSqlQuery balance(database);
    balance.prepare(QStringLiteral("SELECT balance_cents FROM users WHERE id = ?"));
    balance.addBindValue(user->id);
    if (!balance.exec() || !balance.next()) {
        const ServiceResult result = databaseFailure(balance);
        database.rollback();
        return result;
    }
    const qint64 originalBalance = balance.value(0).toLongLong();
    const bool paid = originalBalance >= amountCents;
    if (paid && amountCents > 0) {
        QSqlQuery deduct(database);
        deduct.prepare(QStringLiteral(
            "UPDATE users SET balance_cents = balance_cents - ?, updated_at = ? WHERE id = ?"));
        deduct.addBindValue(amountCents);
        deduct.addBindValue(endedAtText);
        deduct.addBindValue(user->id);
        if (!deduct.exec()) {
            const ServiceResult result = databaseFailure(deduct);
            database.rollback();
            return result;
        }
    }

    const QString orderNo = QStringLiteral("EV%1%2")
        .arg(endedAt.toString(QStringLiteral("yyyyMMddHHmmsszzz")))
        .arg(sessionId, 6, 10, QLatin1Char('0'));
    QSqlQuery insertOrder(database);
    insertOrder.prepare(QStringLiteral(
        "INSERT INTO orders(order_no, charging_session_id, user_id, station_id, charger_id, "
        "energy_wh, amount_cents, status, created_at, paid_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insertOrder.addBindValue(orderNo);
    insertOrder.addBindValue(sessionId);
    insertOrder.addBindValue(user->id);
    insertOrder.addBindValue(stationId);
    insertOrder.addBindValue(chargerId);
    insertOrder.addBindValue(energyWh);
    insertOrder.addBindValue(amountCents);
    insertOrder.addBindValue(paid ? QStringLiteral("paid") : QStringLiteral("pending"));
    insertOrder.addBindValue(endedAtText);
    insertOrder.addBindValue(paid ? QVariant(endedAtText) : QVariant{});
    if (!insertOrder.exec()) {
        const ServiceResult result = databaseFailure(insertOrder);
        database.rollback();
        return result;
    }
    const qint64 orderId = insertOrder.lastInsertId().toLongLong();
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                      database.lastError().text());
    }

    return ServiceResult::success({
        {QStringLiteral("order"), QJsonObject{
             {QStringLiteral("id"), jsonId(orderId)},
             {QStringLiteral("orderNo"), orderNo},
             {QStringLiteral("sessionId"), jsonId(sessionId)},
             {QStringLiteral("energyWh"), jsonId(energyWh)},
             {QStringLiteral("amountCents"), jsonId(amountCents)},
             {QStringLiteral("status"), paid ? QStringLiteral("paid") : QStringLiteral("pending")},
             {QStringLiteral("createdAt"), endedAtText},
             {QStringLiteral("balanceCents"), jsonId(paid ? originalBalance - amountCents
                                                           : originalBalance)}
         }}
    });
}

ServiceResult BusinessService::listOrders(const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT o.id, o.order_no, o.energy_wh, o.amount_cents, o.status, o.created_at, o.paid_at, "
        "s.name, c.code, o.charging_session_id FROM orders o "
        "JOIN stations s ON s.id = o.station_id JOIN chargers c ON c.id = o.charger_id "
        "WHERE o.user_id = ? ORDER BY o.id DESC LIMIT 100"));
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);

    QJsonArray orders;
    while (query.next()) {
        orders.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("orderNo"), query.value(1).toString()},
            {QStringLiteral("energyWh"), jsonId(query.value(2).toLongLong())},
            {QStringLiteral("amountCents"), jsonId(query.value(3).toLongLong())},
            {QStringLiteral("status"), query.value(4).toString()},
            {QStringLiteral("createdAt"), query.value(5).toString()},
            {QStringLiteral("paidAt"), query.value(6).toString()},
            {QStringLiteral("stationName"), query.value(7).toString()},
            {QStringLiteral("chargerCode"), query.value(8).toString()},
            {QStringLiteral("sessionId"), jsonId(query.value(9).toLongLong())}
        });
    }
    return ServiceResult::success({{QStringLiteral("orders"), orders}});
}

ServiceResult BusinessService::getOrder(const QJsonObject &payload, const QString &token)
{
    ServiceResult failure;
    const auto user = authenticate(token, &failure);
    if (!user) return failure;
    const qint64 orderId = static_cast<qint64>(payload.value(QStringLiteral("orderId")).toDouble());
    if (orderId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("orderId 无效"));
    }

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT o.id, o.order_no, o.energy_wh, o.amount_cents, o.status, o.created_at, o.paid_at, "
        "s.id, s.name, s.address, c.id, c.code, cs.started_at, cs.ended_at, "
        "cs.price_cents_per_kwh FROM orders o "
        "JOIN stations s ON s.id = o.station_id JOIN chargers c ON c.id = o.charger_id "
        "JOIN charging_sessions cs ON cs.id = o.charging_session_id "
        "WHERE o.id = ? AND o.user_id = ?"));
    query.addBindValue(orderId);
    query.addBindValue(user->id);
    if (!query.exec()) return databaseFailure(query);
    if (!query.next()) {
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("订单不存在"));
    }
    return ServiceResult::success({
        {QStringLiteral("order"), QJsonObject{
             {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
             {QStringLiteral("orderNo"), query.value(1).toString()},
             {QStringLiteral("energyWh"), jsonId(query.value(2).toLongLong())},
             {QStringLiteral("amountCents"), jsonId(query.value(3).toLongLong())},
             {QStringLiteral("status"), query.value(4).toString()},
             {QStringLiteral("createdAt"), query.value(5).toString()},
             {QStringLiteral("paidAt"), query.value(6).toString()},
             {QStringLiteral("stationId"), jsonId(query.value(7).toLongLong())},
             {QStringLiteral("stationName"), query.value(8).toString()},
             {QStringLiteral("stationAddress"), query.value(9).toString()},
             {QStringLiteral("chargerId"), jsonId(query.value(10).toLongLong())},
             {QStringLiteral("chargerCode"), query.value(11).toString()},
             {QStringLiteral("startedAt"), query.value(12).toString()},
             {QStringLiteral("endedAt"), query.value(13).toString()},
             {QStringLiteral("priceCentsPerKwh"), query.value(14).toInt()}
         }}
    });
}

ServiceResult BusinessService::adminDashboard(const QString &token)
{
    // 运营首页在服务端聚合核心指标，客户端只负责展示，避免重复口径。
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;

    QSqlError lastError;
    auto scalar = [this, &lastError](const QString &sql,
                                    const QVariantList &binds = {}) -> qint64 {
        QSqlQuery query(database_.connection());
        query.prepare(sql);
        for (const QVariant &bind : binds) query.addBindValue(bind);
        if (!query.exec() || !query.next()) {
            lastError = query.lastError();
            return 0;
        }
        return query.value(0).toLongLong();
    };

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString todayStart = QDateTime(now.date(), QTime(0, 0), Qt::UTC)
                                   .toString(Qt::ISODateWithMs);
    const QString monthStart = QDateTime(QDate(now.date().year(), now.date().month(), 1),
                                         QTime(0, 0), Qt::UTC).toString(Qt::ISODateWithMs);
    const qint64 userCount = scalar(QStringLiteral("SELECT COUNT(*) FROM users WHERE role = 'user'"));
    const qint64 stationCount = scalar(QStringLiteral("SELECT COUNT(*) FROM stations WHERE status = 'active'"));
    const qint64 chargerCount = scalar(QStringLiteral("SELECT COUNT(*) FROM chargers"));
    const qint64 idleCount = scalar(QStringLiteral("SELECT COUNT(*) FROM chargers WHERE status = 'idle'"));
    const qint64 chargingCount = scalar(QStringLiteral("SELECT COUNT(*) FROM chargers WHERE status = 'charging'"));
    const qint64 faultCount = scalar(QStringLiteral("SELECT COUNT(*) FROM chargers WHERE status = 'fault'"));
    const qint64 todayOrders = scalar(
        QStringLiteral("SELECT COUNT(*) FROM orders WHERE created_at >= ?"), {todayStart});
    const qint64 todayRevenue = scalar(
        QStringLiteral("SELECT COALESCE(SUM(amount_cents), 0) FROM orders "
                       "WHERE status = 'paid' AND created_at >= ?"), {todayStart});
    const qint64 monthRevenue = scalar(
        QStringLiteral("SELECT COALESCE(SUM(amount_cents), 0) FROM orders "
                       "WHERE status = 'paid' AND created_at >= ?"), {monthStart});
    const qint64 totalRevenue = scalar(
        QStringLiteral("SELECT COALESCE(SUM(amount_cents), 0) FROM orders WHERE status = 'paid'"));
    if (lastError.isValid()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), lastError.text());
    }

    const QString trendStartDate = now.date().addDays(-29).toString(Qt::ISODate);
    QSqlQuery trendQuery(database_.connection());
    trendQuery.prepare(QStringLiteral(
        "SELECT substr(created_at, 1, 10) AS day, COUNT(*), "
        "COALESCE(SUM(CASE WHEN status = 'paid' THEN amount_cents ELSE 0 END), 0) "
        "FROM orders WHERE created_at >= ? GROUP BY day ORDER BY day"));
    trendQuery.addBindValue(trendStartDate);
    if (!trendQuery.exec()) return databaseFailure(trendQuery);
    QHash<QString, QPair<qint64, qint64>> trendValues;
    while (trendQuery.next()) {
        trendValues.insert(trendQuery.value(0).toString(),
                           {trendQuery.value(1).toLongLong(), trendQuery.value(2).toLongLong()});
    }
    auto makeTrend = [&now, &trendValues](int days) {
        QJsonArray result;
        for (int offset = days - 1; offset >= 0; --offset) {
        const QString day = now.date().addDays(-offset).toString(Qt::ISODate);
        const auto value = trendValues.value(day, {0, 0});
            result.append(QJsonObject{
                {QStringLiteral("date"), day},
                {QStringLiteral("orderCount"), jsonId(value.first)},
                {QStringLiteral("revenueCents"), jsonId(value.second)}
            });
        }
        return result;
    };

    return ServiceResult::success({
        {QStringLiteral("userCount"), jsonId(userCount)},
        {QStringLiteral("stationCount"), jsonId(stationCount)},
        {QStringLiteral("chargerCount"), jsonId(chargerCount)},
        {QStringLiteral("idleChargerCount"), jsonId(idleCount)},
        {QStringLiteral("chargingChargerCount"), jsonId(chargingCount)},
        {QStringLiteral("faultChargerCount"), jsonId(faultCount)},
        {QStringLiteral("todayOrderCount"), jsonId(todayOrders)},
        {QStringLiteral("todayRevenueCents"), jsonId(todayRevenue)},
        {QStringLiteral("monthRevenueCents"), jsonId(monthRevenue)},
        {QStringLiteral("totalRevenueCents"), jsonId(totalRevenue)},
        {QStringLiteral("sevenDayTrend"), makeTrend(7)},
        {QStringLiteral("thirtyDayTrend"), makeTrend(30)}
    });
}

ServiceResult BusinessService::adminAnalytics(const QString &token)
{
    // 统计接口返回 7/30 日趋势、累计充电指标和站点排名。
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;

    const QDate today = QDateTime::currentDateTimeUtc().date();
    const QString startDate = today.addDays(-29).toString(Qt::ISODate);
    QSqlDatabase database = database_.connection();

    QSqlQuery dailyQuery(database);
    dailyQuery.prepare(QStringLiteral(
        "SELECT substr(created_at, 1, 10), COUNT(*), COALESCE(SUM(energy_wh), 0), "
        "COALESCE(SUM(CASE WHEN status = 'paid' THEN amount_cents ELSE 0 END), 0) "
        "FROM orders WHERE created_at >= ? GROUP BY substr(created_at, 1, 10)"));
    dailyQuery.addBindValue(startDate);
    if (!dailyQuery.exec()) return databaseFailure(dailyQuery);
    QHash<QString, QJsonObject> dailyValues;
    while (dailyQuery.next()) {
        dailyValues.insert(dailyQuery.value(0).toString(), QJsonObject{
            {QStringLiteral("orderCount"), jsonId(dailyQuery.value(1).toLongLong())},
            {QStringLiteral("energyWh"), jsonId(dailyQuery.value(2).toLongLong())},
            {QStringLiteral("revenueCents"), jsonId(dailyQuery.value(3).toLongLong())}
        });
    }
    QJsonArray dailyTrend;
    qint64 totalOrders = 0;
    qint64 totalEnergyWh = 0;
    qint64 totalRevenueCents = 0;
    for (int offset = 29; offset >= 0; --offset) {
        const QString date = today.addDays(-offset).toString(Qt::ISODate);
        const QJsonObject values = dailyValues.value(date);
        const qint64 orders = jsonInteger(values.value(QStringLiteral("orderCount")));
        const qint64 energy = jsonInteger(values.value(QStringLiteral("energyWh")));
        const qint64 revenue = jsonInteger(values.value(QStringLiteral("revenueCents")));
        totalOrders += orders;
        totalEnergyWh += energy;
        totalRevenueCents += revenue;
        dailyTrend.append(QJsonObject{
            {QStringLiteral("date"), date},
            {QStringLiteral("orderCount"), jsonId(orders)},
            {QStringLiteral("energyWh"), jsonId(energy)},
            {QStringLiteral("revenueCents"), jsonId(revenue)}
        });
    }

    QSqlQuery stationQuery(database);
    stationQuery.prepare(QStringLiteral(
        "SELECT s.id, s.name, COUNT(o.id), COALESCE(SUM(o.energy_wh), 0), "
        "COALESCE(SUM(CASE WHEN o.status = 'paid' THEN o.amount_cents ELSE 0 END), 0) "
        "FROM stations s LEFT JOIN orders o ON o.station_id = s.id AND o.created_at >= ? "
        "GROUP BY s.id ORDER BY 5 DESC, 3 DESC LIMIT 10"));
    stationQuery.addBindValue(startDate);
    if (!stationQuery.exec()) return databaseFailure(stationQuery);
    QJsonArray stationRanking;
    while (stationQuery.next()) {
        stationRanking.append(QJsonObject{
            {QStringLiteral("stationId"), jsonId(stationQuery.value(0).toLongLong())},
            {QStringLiteral("stationName"), stationQuery.value(1).toString()},
            {QStringLiteral("orderCount"), jsonId(stationQuery.value(2).toLongLong())},
            {QStringLiteral("energyWh"), jsonId(stationQuery.value(3).toLongLong())},
            {QStringLiteral("revenueCents"), jsonId(stationQuery.value(4).toLongLong())}
        });
    }

    QSqlQuery faultQuery(database);
    faultQuery.prepare(QStringLiteral(
        "SELECT substr(reported_at, 1, 10), COUNT(*) FROM fault_reports "
        "WHERE reported_at >= ? GROUP BY substr(reported_at, 1, 10)"));
    faultQuery.addBindValue(startDate);
    if (!faultQuery.exec()) return databaseFailure(faultQuery);
    QHash<QString, qint64> faultValues;
    while (faultQuery.next()) faultValues.insert(faultQuery.value(0).toString(), faultQuery.value(1).toLongLong());
    QJsonArray faultTrend;
    for (int offset = 29; offset >= 0; --offset) {
        const QString date = today.addDays(-offset).toString(Qt::ISODate);
        faultTrend.append(QJsonObject{{QStringLiteral("date"), date},
                                      {QStringLiteral("count"), jsonId(faultValues.value(date))}});
    }

    QSqlQuery statusQuery(database);
    if (!statusQuery.exec(QStringLiteral("SELECT status, COUNT(*) FROM chargers GROUP BY status"))) {
        return databaseFailure(statusQuery);
    }
    QJsonArray chargerStatuses;
    qint64 chargerCount = 0;
    while (statusQuery.next()) {
        const qint64 count = statusQuery.value(1).toLongLong();
        chargerCount += count;
        chargerStatuses.append(QJsonObject{
            {QStringLiteral("status"), statusQuery.value(0).toString()},
            {QStringLiteral("count"), jsonId(count)}
        });
    }

    return ServiceResult::success({
        {QStringLiteral("generatedAt"), utcNow()},
        {QStringLiteral("dataNotice"), QStringLiteral("教学演示数据，不代表真实运营结果")},
        {QStringLiteral("periodDays"), 30},
        {QStringLiteral("summary"), QJsonObject{
             {QStringLiteral("orderCount"), jsonId(totalOrders)},
             {QStringLiteral("energyWh"), jsonId(totalEnergyWh)},
             {QStringLiteral("revenueCents"), jsonId(totalRevenueCents)},
             {QStringLiteral("chargerCount"), jsonId(chargerCount)}
         }},
        {QStringLiteral("dailyTrend"), dailyTrend},
        {QStringLiteral("stationRanking"), stationRanking},
        {QStringLiteral("chargerStatuses"), chargerStatuses},
        {QStringLiteral("faultTrend"), faultTrend}
    });
}

ServiceResult BusinessService::adminGenerateDemoHistory(const QJsonObject &payload,
                                                        const QString &token)
{
    ServiceResult failure;
    const auto admin = authenticateAdmin(token, &failure);
    if (!admin) return failure;
    if (!payload.value(QStringLiteral("confirmed")).toBool(false)) {
        return ServiceResult::failure(QStringLiteral("CONFIRMATION_REQUIRED"),
                                      QStringLiteral("生成演示数据前必须明确确认"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    auto rollback = [&database](const QSqlQuery &query) {
        const ServiceResult result = databaseFailure(query);
        database.rollback();
        return result;
    };

    QSqlQuery userQuery(database);
    userQuery.prepare(QStringLiteral("SELECT id FROM users WHERE username = 'demo'"));
    if (!userQuery.exec() || !userQuery.next()) return rollback(userQuery);
    const qint64 userId = userQuery.value(0).toLongLong();

    struct DemoCharger { qint64 id; qint64 stationId; int price; };
    QList<DemoCharger> chargers;
    QSqlQuery chargerQuery(database);
    if (!chargerQuery.exec(QStringLiteral(
            "SELECT c.id, c.station_id, t.price_cents_per_kwh FROM chargers c "
            "JOIN tariffs t ON t.id = c.tariff_id ORDER BY c.id"))) return rollback(chargerQuery);
    while (chargerQuery.next()) {
        chargers.append({chargerQuery.value(0).toLongLong(), chargerQuery.value(1).toLongLong(),
                         chargerQuery.value(2).toInt()});
    }
    if (chargers.isEmpty()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"), QStringLiteral("没有可用于演示的充电桩"));
    }

    const QDate today = QDateTime::currentDateTimeUtc().date();
    int inserted = 0;
    for (int offset = 29; offset >= 0; --offset) {
        const QDate date = today.addDays(-offset);
        const int dailyCount = 2 + ((offset * 7) % 4);
        for (int index = 0; index < dailyCount; ++index) {
            const QString orderNo = QStringLiteral("DEMO-%1-%2")
                                        .arg(date.toString(QStringLiteral("yyyyMMdd")))
                                        .arg(index + 1, 2, 10, QLatin1Char('0'));
            QSqlQuery exists(database);
            exists.prepare(QStringLiteral("SELECT 1 FROM orders WHERE order_no = ?"));
            exists.addBindValue(orderNo);
            if (!exists.exec()) return rollback(exists);
            if (exists.next()) continue;

            const DemoCharger charger = chargers.at((offset + index * 3) % chargers.size());
            const int hour = 7 + ((offset * 5 + index * 3) % 15);
            const int minutes = 20 + ((offset * 11 + index * 17) % 70);
            const QDateTime started(date, QTime(hour, (index * 13) % 60), Qt::UTC);
            const QDateTime ended = started.addSecs(minutes * 60);
            const qint64 energyWh = 3500 + ((offset * 977 + index * 1873) % 22000);
            const qint64 amountCents = qRound64(energyWh * charger.price / 1000.0);

            QSqlQuery session(database);
            session.prepare(QStringLiteral(
                "INSERT INTO charging_sessions(user_id, charger_id, reservation_id, status, "
                "started_at, ended_at, energy_wh, price_cents_per_kwh, amount_cents) "
                "VALUES(?, ?, NULL, 'finished', ?, ?, ?, ?, ?)"));
            session.addBindValue(userId);
            session.addBindValue(charger.id);
            session.addBindValue(started.toString(Qt::ISODateWithMs));
            session.addBindValue(ended.toString(Qt::ISODateWithMs));
            session.addBindValue(energyWh);
            session.addBindValue(charger.price);
            session.addBindValue(amountCents);
            if (!session.exec()) return rollback(session);

            QSqlQuery order(database);
            order.prepare(QStringLiteral(
                "INSERT INTO orders(order_no, charging_session_id, user_id, station_id, charger_id, "
                "energy_wh, amount_cents, status, created_at, paid_at) "
                "VALUES(?, ?, ?, ?, ?, ?, ?, 'paid', ?, ?)"));
            order.addBindValue(orderNo);
            order.addBindValue(session.lastInsertId());
            order.addBindValue(userId);
            order.addBindValue(charger.stationId);
            order.addBindValue(charger.id);
            order.addBindValue(energyWh);
            order.addBindValue(amountCents);
            order.addBindValue(ended.toString(Qt::ISODateWithMs));
            order.addBindValue(ended.toString(Qt::ISODateWithMs));
            if (!order.exec()) return rollback(order);
            ++inserted;
        }
    }

    QSqlQuery audit(database);
    audit.prepare(QStringLiteral(
        "INSERT INTO audit_logs(actor_user_id, action, entity_type, entity_id, detail_json, created_at) "
        "VALUES(?, 'generate_demo_history', 'analytics', '', ?, ?)"));
    audit.addBindValue(admin->id);
    audit.addBindValue(QStringLiteral("{\"insertedOrders\":%1}").arg(inserted));
    audit.addBindValue(utcNow());
    if (!audit.exec()) return rollback(audit);
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), database.lastError().text());
    }
    return ServiceResult::success({
        {QStringLiteral("insertedOrders"), inserted},
        {QStringLiteral("dataNotice"), QStringLiteral("已生成可重复的教学演示历史数据")}
    });
}

ServiceResult BusinessService::adminListStations(const QString &token)
{
    // 以下为管理员资源维护接口，所有入口先统一验证管理员权限。
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;

    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT s.id, s.name, s.region, s.address, s.longitude, s.latitude, "
        "s.business_hours, s.status, COUNT(c.id) AS charger_count, "
        "COALESCE(SUM(CASE WHEN c.status = 'idle' THEN 1 ELSE 0 END), 0) AS idle_count, "
        "COALESCE(SUM(CASE WHEN c.status NOT IN ('offline', 'disabled') THEN 1 ELSE 0 END), 0) "
        "AS online_count, "
        "COALESCE(MIN(t.price_cents_per_kwh), 0) AS minimum_price "
        "FROM stations s LEFT JOIN chargers c ON c.station_id = s.id "
        "LEFT JOIN tariffs t ON t.id = c.tariff_id GROUP BY s.id ORDER BY s.id"));
    if (!query.exec()) return databaseFailure(query);
    QJsonArray stations;
    while (query.next()) stations.append(stationFromQuery(query));
    return ServiceResult::success({{QStringLiteral("stations"), stations}});
}

ServiceResult BusinessService::adminSaveStation(const QJsonObject &payload,
                                                const QString &token)
{
    ServiceResult failure;
    const auto admin = authenticateAdmin(token, &failure);
    if (!admin) return failure;

    const qint64 stationId = static_cast<qint64>(payload.value(QStringLiteral("id")).toDouble());
    const QString name = payload.value(QStringLiteral("name")).toString().trimmed();
    const QString region = payload.value(QStringLiteral("region")).toString().trimmed();
    const QString address = payload.value(QStringLiteral("address")).toString().trimmed();
    const double longitude = payload.value(QStringLiteral("longitude")).toDouble();
    const double latitude = payload.value(QStringLiteral("latitude")).toDouble();
    const QString businessHours = payload.value(QStringLiteral("businessHours"))
                                      .toString(QStringLiteral("00:00-24:00")).trimmed();
    const QString status = payload.value(QStringLiteral("status"))
                               .toString(QStringLiteral("active"));
    const int chargerCount = payload.value(QStringLiteral("chargerCount")).toInt(0);
    const double defaultPowerKw = payload.value(QStringLiteral("defaultPowerKw")).toDouble(7.0);
    const qint64 tariffId = static_cast<qint64>(
        payload.value(QStringLiteral("tariffId")).toDouble(1));
    if (name.isEmpty() || address.isEmpty()
        || longitude < -180.0 || longitude > 180.0
        || latitude < -90.0 || latitude > 90.0
        || chargerCount < 0 || chargerCount > 50 || defaultPowerKw <= 0 || tariffId <= 0
        || !QStringList{QStringLiteral("active"), QStringLiteral("disabled")}.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("站点名称、地址或状态无效"));
    }

    const QString now = utcNow();
    QSqlQuery query(database_.connection());
    if (stationId > 0) {
        query.prepare(QStringLiteral(
            "UPDATE stations SET name = ?, region = ?, address = ?, longitude = ?, latitude = ?, "
            "business_hours = ?, status = ?, updated_at = ? WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(region);
        query.addBindValue(address);
        query.addBindValue(longitude);
        query.addBindValue(latitude);
        query.addBindValue(businessHours);
        query.addBindValue(status);
        query.addBindValue(now);
        query.addBindValue(stationId);
        if (!query.exec()) return databaseFailure(query);
        if (query.numRowsAffected() != 1) {
            return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                          QStringLiteral("站点不存在"));
        }
        return ServiceResult::success({{QStringLiteral("stationId"), jsonId(stationId)}});
    }

    QSqlDatabase database = database_.connection();
    QString transactionError;
    if (!beginImmediate(database, &transactionError)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), transactionError);
    }
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO stations(name, region, address, longitude, latitude, business_hours, status, "
        "created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(name);
    insert.addBindValue(region);
    insert.addBindValue(address);
    insert.addBindValue(longitude);
    insert.addBindValue(latitude);
    insert.addBindValue(businessHours);
    insert.addBindValue(status);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        const ServiceResult result = databaseFailure(insert);
        database.rollback();
        return result;
    }
    const qint64 createdStationId = insert.lastInsertId().toLongLong();
    for (int index = 1; index <= chargerCount; ++index) {
        QSqlQuery charger(database);
        charger.prepare(QStringLiteral(
            "INSERT INTO chargers(station_id, code, connector_type, rated_power_kw, status, "
            "tariff_id, created_at, updated_at) VALUES(?, ?, 'GB/T', ?, 'idle', ?, ?, ?)"));
        charger.addBindValue(createdStationId);
        charger.addBindValue(QStringLiteral("ST%1-%2")
                                 .arg(createdStationId, 4, 10, QLatin1Char('0'))
                                 .arg(index, 3, 10, QLatin1Char('0')));
        charger.addBindValue(defaultPowerKw);
        charger.addBindValue(tariffId);
        charger.addBindValue(now);
        charger.addBindValue(now);
        if (!charger.exec()) {
            const ServiceResult result = databaseFailure(charger);
            database.rollback();
            return result;
        }
    }
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), database.lastError().text());
    }
    return ServiceResult::success({
        {QStringLiteral("stationId"), jsonId(createdStationId)},
        {QStringLiteral("createdChargers"), chargerCount}
    });
}

ServiceResult BusinessService::adminListChargers(const QJsonObject &payload,
                                                 const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 stationId = static_cast<qint64>(payload.value(QStringLiteral("stationId")).toDouble());

    QString sql = QStringLiteral(
        "SELECT c.id, c.station_id, s.name, c.code, c.connector_type, c.rated_power_kw, c.status, "
        "c.tariff_id, t.name, t.price_cents_per_kwh, c.updated_at, "
        "(SELECT COUNT(*) FROM charging_sessions cs WHERE cs.charger_id = c.id) AS session_count, "
        "(SELECT COALESCE(SUM(CAST((julianday(cs.ended_at) - julianday(cs.started_at)) * 86400 "
        "AS INTEGER)), 0) FROM charging_sessions cs WHERE cs.charger_id = c.id "
        "AND cs.ended_at IS NOT NULL) AS total_duration_seconds FROM chargers c "
        "JOIN stations s ON s.id = c.station_id JOIN tariffs t ON t.id = c.tariff_id ");
    if (stationId > 0) sql += QStringLiteral("WHERE c.station_id = ? ");
    sql += QStringLiteral("ORDER BY c.id");
    QSqlQuery query(database_.connection());
    query.prepare(sql);
    if (stationId > 0) query.addBindValue(stationId);
    if (!query.exec()) return databaseFailure(query);

    QJsonArray chargers;
    while (query.next()) {
        chargers.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("stationId"), jsonId(query.value(1).toLongLong())},
            {QStringLiteral("stationName"), query.value(2).toString()},
            {QStringLiteral("code"), query.value(3).toString()},
            {QStringLiteral("connectorType"), query.value(4).toString()},
            {QStringLiteral("ratedPowerKw"), query.value(5).toDouble()},
            {QStringLiteral("status"), query.value(6).toString()},
            {QStringLiteral("tariffId"), jsonId(query.value(7).toLongLong())},
            {QStringLiteral("tariffName"), query.value(8).toString()},
            {QStringLiteral("priceCentsPerKwh"), query.value(9).toInt()},
            {QStringLiteral("updatedAt"), query.value(10).toString()},
            {QStringLiteral("sessionCount"), jsonId(query.value(11).toLongLong())},
            {QStringLiteral("totalDurationSeconds"), jsonId(query.value(12).toLongLong())}
        });
    }
    return ServiceResult::success({{QStringLiteral("chargers"), chargers}});
}

ServiceResult BusinessService::adminSaveCharger(const QJsonObject &payload,
                                                const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 chargerId = static_cast<qint64>(payload.value(QStringLiteral("id")).toDouble());
    const qint64 stationId = static_cast<qint64>(payload.value(QStringLiteral("stationId")).toDouble());
    const qint64 tariffId = static_cast<qint64>(payload.value(QStringLiteral("tariffId")).toDouble());
    const QString code = payload.value(QStringLiteral("code")).toString().trimmed();
    const QString connectorType = payload.value(QStringLiteral("connectorType"))
                                      .toString(QStringLiteral("GB/T")).trimmed();
    const double ratedPowerKw = payload.value(QStringLiteral("ratedPowerKw")).toDouble();
    const QString status = payload.value(QStringLiteral("status"))
                               .toString(QStringLiteral("idle"));
    const QStringList allowedStatuses{QStringLiteral("idle"), QStringLiteral("reserved"),
                                      QStringLiteral("charging"), QStringLiteral("fault"),
                                      QStringLiteral("offline"), QStringLiteral("disabled")};
    if (stationId <= 0 || tariffId <= 0 || code.isEmpty() || ratedPowerKw <= 0
        || !allowedStatuses.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("充电桩参数无效"));
    }
    if (chargerId <= 0 && (status == QStringLiteral("reserved")
                           || status == QStringLiteral("charging"))) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("新充电桩不能直接设为占用状态"));
    }

    const QString now = utcNow();
    QSqlQuery query(database_.connection());
    if (chargerId > 0) {
        QSqlQuery current(database_.connection());
        current.prepare(QStringLiteral("SELECT status FROM chargers WHERE id = ?"));
        current.addBindValue(chargerId);
        if (!current.exec()) return databaseFailure(current);
        if (!current.next()) {
            return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                          QStringLiteral("充电桩不存在"));
        }
        const QString currentStatus = current.value(0).toString();
        if ((currentStatus == QStringLiteral("reserved") || currentStatus == QStringLiteral("charging"))
            && status != currentStatus) {
            return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                          QStringLiteral("占用中的充电桩不能直接修改状态"));
        }
        query.prepare(QStringLiteral(
            "UPDATE chargers SET station_id = ?, code = ?, connector_type = ?, rated_power_kw = ?, "
            "status = ?, tariff_id = ?, updated_at = ? WHERE id = ?"));
        query.addBindValue(stationId);
        query.addBindValue(code);
        query.addBindValue(connectorType);
        query.addBindValue(ratedPowerKw);
        query.addBindValue(status);
        query.addBindValue(tariffId);
        query.addBindValue(now);
        query.addBindValue(chargerId);
        if (!query.exec()) return databaseFailure(query);
        return ServiceResult::success({{QStringLiteral("chargerId"), jsonId(chargerId)}});
    }

    query.prepare(QStringLiteral(
        "INSERT INTO chargers(station_id, code, connector_type, rated_power_kw, status, tariff_id, "
        "created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(stationId);
    query.addBindValue(code);
    query.addBindValue(connectorType);
    query.addBindValue(ratedPowerKw);
    query.addBindValue(status);
    query.addBindValue(tariffId);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) return databaseFailure(query);
    return ServiceResult::success({
        {QStringLiteral("chargerId"), jsonId(query.lastInsertId().toLongLong())}
    });
}

ServiceResult BusinessService::adminSetChargerStatus(const QJsonObject &payload,
                                                     const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 chargerId = static_cast<qint64>(payload.value(QStringLiteral("chargerId")).toDouble());
    const QString status = payload.value(QStringLiteral("status")).toString();
    const QStringList allowed{QStringLiteral("idle"), QStringLiteral("fault"),
                              QStringLiteral("offline"), QStringLiteral("disabled")};
    if (chargerId <= 0 || !allowed.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("充电桩编号或目标状态无效"));
    }
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "UPDATE chargers SET status = ?, updated_at = ? "
        "WHERE id = ? AND status NOT IN ('reserved', 'charging')"));
    query.addBindValue(status);
    query.addBindValue(utcNow());
    query.addBindValue(chargerId);
    if (!query.exec()) return databaseFailure(query);
    if (query.numRowsAffected() != 1) {
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("充电桩不存在或正在被占用"));
    }
    return ServiceResult::success({{QStringLiteral("chargerId"), jsonId(chargerId)}});
}

ServiceResult BusinessService::adminRestartCharger(const QJsonObject &payload,
                                                    const QString &token)
{
    // 远程重启写入可追溯操作记录，并按设备当前状态给出模拟执行结果。
    ServiceResult failure;
    const auto admin = authenticateAdmin(token, &failure);
    if (!admin) return failure;
    const qint64 chargerId = static_cast<qint64>(
        payload.value(QStringLiteral("chargerId")).toDouble());
    const bool simulateFailure = payload.value(QStringLiteral("simulateFailure")).toBool(false);
    if (chargerId <= 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("充电桩编号无效"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    QSqlQuery current(database);
    current.prepare(QStringLiteral("SELECT code, status FROM chargers WHERE id = ?"));
    current.addBindValue(chargerId);
    if (!current.exec() || !current.next()) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("充电桩不存在"));
    }
    const QString code = current.value(0).toString();
    const QString previousStatus = current.value(1).toString();
    if (previousStatus != QStringLiteral("fault")) {
        database.rollback();
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("仅故障状态的充电桩可以远程重启"));
    }

    const bool success = !simulateFailure;
    const QString resultStatus = success ? QStringLiteral("idle") : previousStatus;
    const QString message = success
        ? QStringLiteral("模拟重启成功，设备恢复为空闲")
        : QStringLiteral("模拟重启失败，设备保持故障状态");
    if (success) {
        QSqlQuery update(database);
        update.prepare(QStringLiteral(
            "UPDATE chargers SET status = 'idle', updated_at = ? WHERE id = ? AND status = 'fault'"));
        update.addBindValue(utcNow());
        update.addBindValue(chargerId);
        if (!update.exec() || update.numRowsAffected() != 1) {
            const ServiceResult result = databaseFailure(update);
            database.rollback();
            return result;
        }
    }

    QSqlQuery log(database);
    log.prepare(QStringLiteral(
        "INSERT INTO charger_operation_logs(charger_id, operator_user_id, operation, "
        "previous_status, result_status, success, message, created_at) "
        "VALUES(?, ?, 'restart', ?, ?, ?, ?, ?)"));
    log.addBindValue(chargerId);
    log.addBindValue(admin->id);
    log.addBindValue(previousStatus);
    log.addBindValue(resultStatus);
    log.addBindValue(success ? 1 : 0);
    log.addBindValue(message);
    log.addBindValue(utcNow());
    if (!log.exec()) {
        const ServiceResult result = databaseFailure(log);
        database.rollback();
        return result;
    }
    const qint64 operationId = log.lastInsertId().toLongLong();
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), database.lastError().text());
    }
    return ServiceResult::success({
        {QStringLiteral("operationId"), jsonId(operationId)},
        {QStringLiteral("chargerId"), jsonId(chargerId)},
        {QStringLiteral("chargerCode"), code},
        {QStringLiteral("success"), success},
        {QStringLiteral("previousStatus"), previousStatus},
        {QStringLiteral("resultStatus"), resultStatus},
        {QStringLiteral("message"), message}
    });
}

ServiceResult BusinessService::adminListChargerOperations(const QJsonObject &payload,
                                                          const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 chargerId = static_cast<qint64>(
        payload.value(QStringLiteral("chargerId")).toDouble());
    QString sql = QStringLiteral(
        "SELECT l.id, l.charger_id, c.code, u.username, l.operation, l.previous_status, "
        "l.result_status, l.success, l.message, l.created_at FROM charger_operation_logs l "
        "JOIN chargers c ON c.id = l.charger_id JOIN users u ON u.id = l.operator_user_id ");
    if (chargerId > 0) sql += QStringLiteral("WHERE l.charger_id = ? ");
    sql += QStringLiteral("ORDER BY l.id DESC LIMIT 200");
    QSqlQuery query(database_.connection());
    query.prepare(sql);
    if (chargerId > 0) query.addBindValue(chargerId);
    if (!query.exec()) return databaseFailure(query);
    QJsonArray operations;
    while (query.next()) {
        operations.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("chargerId"), jsonId(query.value(1).toLongLong())},
            {QStringLiteral("chargerCode"), query.value(2).toString()},
            {QStringLiteral("operator"), query.value(3).toString()},
            {QStringLiteral("operation"), query.value(4).toString()},
            {QStringLiteral("previousStatus"), query.value(5).toString()},
            {QStringLiteral("resultStatus"), query.value(6).toString()},
            {QStringLiteral("success"), query.value(7).toBool()},
            {QStringLiteral("message"), query.value(8).toString()},
            {QStringLiteral("createdAt"), query.value(9).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("operations"), operations}});
}

ServiceResult BusinessService::adminListUsers(const QJsonObject &payload,
                                              const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const QString phoneKeyword = payload.value(QStringLiteral("phoneKeyword"))
                                     .toString().trimmed();
    QString sql = QStringLiteral(
        "SELECT id, username, role, display_name, phone, balance_cents, status, created_at "
        "FROM users ");
    if (!phoneKeyword.isEmpty()) sql += QStringLiteral("WHERE phone LIKE ? ");
    sql += QStringLiteral("ORDER BY id");
    QSqlQuery query(database_.connection());
    query.prepare(sql);
    if (!phoneKeyword.isEmpty()) {
        query.addBindValue(QStringLiteral("%%1%").arg(phoneKeyword));
    }
    if (!query.exec()) return databaseFailure(query);
    QJsonArray users;
    while (query.next()) {
        users.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("username"), query.value(1).toString()},
            {QStringLiteral("role"), query.value(2).toString()},
            {QStringLiteral("displayName"), query.value(3).toString()},
            {QStringLiteral("phone"), query.value(4).toString()},
            {QStringLiteral("balanceCents"), jsonId(query.value(5).toLongLong())},
            {QStringLiteral("status"), query.value(6).toString()},
            {QStringLiteral("createdAt"), query.value(7).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("users"), users}});
}

ServiceResult BusinessService::adminSetUserStatus(const QJsonObject &payload,
                                                  const QString &token)
{
    ServiceResult failure;
    const auto admin = authenticateAdmin(token, &failure);
    if (!admin) return failure;
    const qint64 userId = static_cast<qint64>(payload.value(QStringLiteral("userId")).toDouble());
    const QString status = payload.value(QStringLiteral("status")).toString();
    if (userId <= 0
        || !QStringList{QStringLiteral("active"), QStringLiteral("disabled")}.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("用户编号或状态无效"));
    }
    if (userId == admin->id && status == QStringLiteral("disabled")) {
        return ServiceResult::failure(QStringLiteral("CONFLICT"),
                                      QStringLiteral("不能停用当前管理员账号"));
    }
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral("UPDATE users SET status = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(status);
    query.addBindValue(utcNow());
    query.addBindValue(userId);
    if (!query.exec()) return databaseFailure(query);
    if (query.numRowsAffected() != 1) {
        return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                      QStringLiteral("用户不存在"));
    }
    return ServiceResult::success({{QStringLiteral("userId"), jsonId(userId)}});
}

ServiceResult BusinessService::adminListOrders(const QJsonObject &payload,
                                                const QString &token)
{
    // 订单支持编号、手机号、站点、设备、状态和日期区间组合筛选。
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const QString orderNo = payload.value(QStringLiteral("orderNo")).toString().trimmed();
    const QString phone = payload.value(QStringLiteral("phone")).toString().trimmed();
    const QString stationKeyword = payload.value(QStringLiteral("stationKeyword"))
                                       .toString().trimmed();
    const QString chargerCode = payload.value(QStringLiteral("chargerCode"))
                                    .toString().trimmed();
    const QString status = payload.value(QStringLiteral("status")).toString().trimmed();
    const QString startDate = payload.value(QStringLiteral("startDate")).toString().trimmed();
    const QString endDate = payload.value(QStringLiteral("endDate")).toString().trimmed();
    const QStringList allowedStatuses{QStringLiteral("pending"), QStringLiteral("paid"),
                                      QStringLiteral("cancelled")};
    if (!status.isEmpty() && !allowedStatuses.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("订单状态筛选值无效"));
    }
    const QDate start = QDate::fromString(startDate, Qt::ISODate);
    const QDate end = QDate::fromString(endDate, Qt::ISODate);
    if ((!startDate.isEmpty() && !start.isValid()) || (!endDate.isEmpty() && !end.isValid())
        || (start.isValid() && end.isValid() && start > end)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("订单日期范围无效"));
    }

    QString sql = QStringLiteral(
        "SELECT o.id, o.order_no, u.username, u.phone, s.name, c.code, o.energy_wh, "
        "o.amount_cents, o.status, o.created_at FROM orders o "
        "JOIN users u ON u.id = o.user_id JOIN stations s ON s.id = o.station_id "
        "JOIN chargers c ON c.id = o.charger_id WHERE 1 = 1 ");
    QVariantList binds;
    if (!orderNo.isEmpty()) {
        sql += QStringLiteral("AND o.order_no LIKE ? ");
        binds.append(QStringLiteral("%%1%").arg(orderNo));
    }
    if (!phone.isEmpty()) {
        sql += QStringLiteral("AND u.phone LIKE ? ");
        binds.append(QStringLiteral("%%1%").arg(phone));
    }
    if (!stationKeyword.isEmpty()) {
        sql += QStringLiteral("AND (s.name LIKE ? OR s.address LIKE ?) ");
        binds.append(QStringLiteral("%%1%").arg(stationKeyword));
        binds.append(QStringLiteral("%%1%").arg(stationKeyword));
    }
    if (!chargerCode.isEmpty()) {
        sql += QStringLiteral("AND c.code LIKE ? ");
        binds.append(QStringLiteral("%%1%").arg(chargerCode));
    }
    if (!status.isEmpty()) {
        sql += QStringLiteral("AND o.status = ? ");
        binds.append(status);
    }
    if (start.isValid()) {
        sql += QStringLiteral("AND o.created_at >= ? ");
        binds.append(QDateTime(start, QTime(0, 0), Qt::UTC).toString(Qt::ISODateWithMs));
    }
    if (end.isValid()) {
        sql += QStringLiteral("AND o.created_at < ? ");
        binds.append(QDateTime(end.addDays(1), QTime(0, 0), Qt::UTC).toString(Qt::ISODateWithMs));
    }
    sql += QStringLiteral("ORDER BY o.id DESC LIMIT 500");
    QSqlQuery query(database_.connection());
    query.prepare(sql);
    for (const QVariant &bind : binds) query.addBindValue(bind);
    if (!query.exec()) return databaseFailure(query);
    QJsonArray orders;
    while (query.next()) {
        orders.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("orderNo"), query.value(1).toString()},
            {QStringLiteral("username"), query.value(2).toString()},
            {QStringLiteral("phone"), query.value(3).toString()},
            {QStringLiteral("stationName"), query.value(4).toString()},
            {QStringLiteral("chargerCode"), query.value(5).toString()},
            {QStringLiteral("energyWh"), jsonId(query.value(6).toLongLong())},
            {QStringLiteral("amountCents"), jsonId(query.value(7).toLongLong())},
            {QStringLiteral("status"), query.value(8).toString()},
            {QStringLiteral("createdAt"), query.value(9).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("orders"), orders}});
}

ServiceResult BusinessService::adminListReservations(const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    if (!expireReservations(&failure.errorMessage)) {
        failure.errorCode = QStringLiteral("DATABASE_ERROR");
        return failure;
    }
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT r.id, u.username, s.name, c.code, r.status, r.reserved_at, r.expires_at, "
        "r.completed_at FROM reservations r JOIN users u ON u.id = r.user_id "
        "JOIN chargers c ON c.id = r.charger_id JOIN stations s ON s.id = c.station_id "
        "ORDER BY r.id DESC LIMIT 500"));
    if (!query.exec()) return databaseFailure(query);
    QJsonArray reservations;
    while (query.next()) {
        reservations.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("username"), query.value(1).toString()},
            {QStringLiteral("stationName"), query.value(2).toString()},
            {QStringLiteral("chargerCode"), query.value(3).toString()},
            {QStringLiteral("status"), query.value(4).toString()},
            {QStringLiteral("reservedAt"), query.value(5).toString()},
            {QStringLiteral("expiresAt"), query.value(6).toString()},
            {QStringLiteral("completedAt"), query.value(7).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("reservations"), reservations}});
}

ServiceResult BusinessService::adminListChargingSessions(const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT cs.id, u.username, s.name, c.code, cs.status, cs.started_at, cs.ended_at, "
        "cs.energy_wh, cs.amount_cents, cs.price_cents_per_kwh "
        "FROM charging_sessions cs JOIN users u ON u.id = cs.user_id "
        "JOIN chargers c ON c.id = cs.charger_id JOIN stations s ON s.id = c.station_id "
        "ORDER BY cs.id DESC LIMIT 500"));
    if (!query.exec()) return databaseFailure(query);
    QJsonArray sessions;
    while (query.next()) {
        sessions.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("username"), query.value(1).toString()},
            {QStringLiteral("stationName"), query.value(2).toString()},
            {QStringLiteral("chargerCode"), query.value(3).toString()},
            {QStringLiteral("status"), query.value(4).toString()},
            {QStringLiteral("startedAt"), query.value(5).toString()},
            {QStringLiteral("endedAt"), query.value(6).toString()},
            {QStringLiteral("energyWh"), jsonId(query.value(7).toLongLong())},
            {QStringLiteral("estimatedAmountCents"), jsonId(query.value(8).toLongLong())},
            {QStringLiteral("priceCentsPerKwh"), query.value(9).toInt()}
        });
    }
    return ServiceResult::success({{QStringLiteral("sessions"), sessions}});
}

ServiceResult BusinessService::adminListTariffs(const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT id, name, price_cents_per_kwh, active, created_at, updated_at "
        "FROM tariffs ORDER BY id"));
    if (!query.exec()) return databaseFailure(query);
    QJsonArray tariffs;
    while (query.next()) {
        tariffs.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("name"), query.value(1).toString()},
            {QStringLiteral("priceCentsPerKwh"), query.value(2).toInt()},
            {QStringLiteral("active"), query.value(3).toBool()},
            {QStringLiteral("createdAt"), query.value(4).toString()},
            {QStringLiteral("updatedAt"), query.value(5).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("tariffs"), tariffs}});
}

ServiceResult BusinessService::adminSaveTariff(const QJsonObject &payload,
                                               const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 tariffId = static_cast<qint64>(payload.value(QStringLiteral("id")).toDouble());
    const QString name = payload.value(QStringLiteral("name")).toString().trimmed();
    const int price = payload.value(QStringLiteral("priceCentsPerKwh")).toInt(-1);
    const bool active = payload.value(QStringLiteral("active")).toBool(true);
    if (name.isEmpty() || price < 0) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("价格方案名称或价格无效"));
    }
    const QString now = utcNow();
    QSqlQuery query(database_.connection());
    if (tariffId > 0) {
        query.prepare(QStringLiteral(
            "UPDATE tariffs SET name = ?, price_cents_per_kwh = ?, active = ?, updated_at = ? "
            "WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(price);
        query.addBindValue(active ? 1 : 0);
        query.addBindValue(now);
        query.addBindValue(tariffId);
        if (!query.exec()) return databaseFailure(query);
        if (query.numRowsAffected() != 1) {
            return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                          QStringLiteral("价格方案不存在"));
        }
        return ServiceResult::success({{QStringLiteral("tariffId"), jsonId(tariffId)}});
    }
    query.prepare(QStringLiteral(
        "INSERT INTO tariffs(name, price_cents_per_kwh, active, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(price);
    query.addBindValue(active ? 1 : 0);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) return databaseFailure(query);
    return ServiceResult::success({
        {QStringLiteral("tariffId"), jsonId(query.lastInsertId().toLongLong())}
    });
}

ServiceResult BusinessService::adminListFaults(const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    QSqlQuery query(database_.connection());
    query.prepare(QStringLiteral(
        "SELECT f.id, f.charger_id, c.code, s.name, f.title, f.description, f.status, "
        "f.reported_at, f.resolved_at FROM fault_reports f "
        "JOIN chargers c ON c.id = f.charger_id JOIN stations s ON s.id = c.station_id "
        "ORDER BY CASE f.status WHEN 'open' THEN 0 WHEN 'processing' THEN 1 ELSE 2 END, f.id DESC"));
    if (!query.exec()) return databaseFailure(query);
    QJsonArray faults;
    while (query.next()) {
        faults.append(QJsonObject{
            {QStringLiteral("id"), jsonId(query.value(0).toLongLong())},
            {QStringLiteral("chargerId"), jsonId(query.value(1).toLongLong())},
            {QStringLiteral("chargerCode"), query.value(2).toString()},
            {QStringLiteral("stationName"), query.value(3).toString()},
            {QStringLiteral("title"), query.value(4).toString()},
            {QStringLiteral("description"), query.value(5).toString()},
            {QStringLiteral("status"), query.value(6).toString()},
            {QStringLiteral("reportedAt"), query.value(7).toString()},
            {QStringLiteral("resolvedAt"), query.value(8).toString()}
        });
    }
    return ServiceResult::success({{QStringLiteral("faults"), faults}});
}

ServiceResult BusinessService::adminSaveFault(const QJsonObject &payload,
                                              const QString &token)
{
    ServiceResult failure;
    if (!authenticateAdmin(token, &failure)) return failure;
    const qint64 faultId = static_cast<qint64>(payload.value(QStringLiteral("id")).toDouble());
    const qint64 chargerId = static_cast<qint64>(payload.value(QStringLiteral("chargerId")).toDouble());
    const QString title = payload.value(QStringLiteral("title")).toString().trimmed();
    const QString description = payload.value(QStringLiteral("description")).toString().trimmed();
    const QString status = payload.value(QStringLiteral("status"))
                               .toString(QStringLiteral("open"));
    if (!QStringList{QStringLiteral("open"), QStringLiteral("processing"),
                     QStringLiteral("resolved")}.contains(status)) {
        return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                      QStringLiteral("故障状态无效"));
    }

    QSqlDatabase database = database_.connection();
    QString error;
    if (!beginImmediate(database, &error)) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"), error);
    }
    const QString now = utcNow();
    qint64 effectiveChargerId = chargerId;
    QSqlQuery query(database);
    if (faultId > 0) {
        QSqlQuery current(database);
        current.prepare(QStringLiteral("SELECT charger_id FROM fault_reports WHERE id = ?"));
        current.addBindValue(faultId);
        if (!current.exec() || !current.next()) {
            database.rollback();
            return ServiceResult::failure(QStringLiteral("NOT_FOUND"),
                                          QStringLiteral("故障记录不存在"));
        }
        effectiveChargerId = current.value(0).toLongLong();
        query.prepare(QStringLiteral(
            "UPDATE fault_reports SET title = ?, description = ?, status = ?, resolved_at = ? "
            "WHERE id = ?"));
        query.addBindValue(title);
        query.addBindValue(description);
        query.addBindValue(status);
        query.addBindValue(status == QStringLiteral("resolved") ? QVariant(now) : QVariant{});
        query.addBindValue(faultId);
    } else {
        if (chargerId <= 0 || title.isEmpty()) {
            database.rollback();
            return ServiceResult::failure(QStringLiteral("INVALID_ARGUMENT"),
                                          QStringLiteral("充电桩和故障标题不能为空"));
        }
        query.prepare(QStringLiteral(
            "INSERT INTO fault_reports(charger_id, title, description, status, reported_at, resolved_at) "
            "VALUES(?, ?, ?, ?, ?, ?)"));
        query.addBindValue(chargerId);
        query.addBindValue(title);
        query.addBindValue(description);
        query.addBindValue(status);
        query.addBindValue(now);
        query.addBindValue(status == QStringLiteral("resolved") ? QVariant(now) : QVariant{});
    }
    if (!query.exec()) {
        const ServiceResult result = databaseFailure(query);
        database.rollback();
        return result;
    }
    const qint64 savedFaultId = faultId > 0 ? faultId : query.lastInsertId().toLongLong();

    QSqlQuery chargerUpdate(database);
    if (status == QStringLiteral("resolved")) {
        chargerUpdate.prepare(QStringLiteral(
            "UPDATE chargers SET status = 'idle', updated_at = ? "
            "WHERE id = ? AND status = 'fault'"));
    } else {
        chargerUpdate.prepare(QStringLiteral(
            "UPDATE chargers SET status = 'fault', updated_at = ? "
            "WHERE id = ? AND status NOT IN ('reserved', 'charging')"));
    }
    chargerUpdate.addBindValue(now);
    chargerUpdate.addBindValue(effectiveChargerId);
    if (!chargerUpdate.exec()) {
        const ServiceResult result = databaseFailure(chargerUpdate);
        database.rollback();
        return result;
    }
    if (!database.commit()) {
        return ServiceResult::failure(QStringLiteral("DATABASE_ERROR"),
                                      database.lastError().text());
    }
    return ServiceResult::success({{QStringLiteral("faultId"), jsonId(savedFaultId)}});
}

} // namespace evcs::server
