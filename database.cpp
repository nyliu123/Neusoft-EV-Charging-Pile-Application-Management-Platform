#include "database.h"

#include "security.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace evcs::server {

namespace {

QString utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool execute(QSqlQuery &query, const QString &sql, QString *errorMessage)
{
    if (query.exec(sql)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text() + QStringLiteral(" | SQL: ") + sql;
    }
    return false;
}

} // namespace

Database::Database()
    : connectionName_(QStringLiteral("evcs-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    if (database_.isValid()) {
        database_.close();
        database_ = QSqlDatabase{};
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::initialize(const QString &databasePath,
                          const QString &schemaPath,
                          QString *errorMessage)
{
    // 初始化顺序固定为：打开连接、执行基础结构、增量迁移、写入演示数据。
    if (!openExisting(databasePath, errorMessage)) {
        return false;
    }

    return executeSchema(schemaPath, errorMessage)
        && migrateSchema(errorMessage)
        && seedDefaults(errorMessage);
}

bool Database::openExisting(const QString &databasePath,
                            QString *errorMessage)
{
    // 每个 Database 实例使用唯一连接名，并启用外键、WAL 与忙等待。
    const QFileInfo databaseInfo(databasePath);
    if (!QDir().mkpath(databaseInfo.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建数据库目录：%1")
                                .arg(databaseInfo.absolutePath());
        }
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databaseInfo.absoluteFilePath());
    if (!database_.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开 SQLite 数据库：%1")
                                .arg(database_.lastError().text());
        }
        return false;
    }

    QSqlQuery pragmaQuery(database_);
    if (!execute(pragmaQuery, QStringLiteral("PRAGMA foreign_keys = ON"), errorMessage)
        || !execute(pragmaQuery, QStringLiteral("PRAGMA journal_mode = WAL"), errorMessage)
        || !execute(pragmaQuery, QStringLiteral("PRAGMA busy_timeout = 5000"), errorMessage)) {
        return false;
    }

    return true;
}

QSqlDatabase Database::connection() const
{
    return database_;
}

bool Database::executeSchema(const QString &schemaPath, QString *errorMessage)
{
    // 首次启动执行可重复的建表语句，并登记数据库结构版本。
    QFile schemaFile(schemaPath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取数据库结构文件：%1")
                                .arg(schemaFile.errorString());
        }
        return false;
    }

    const QString schema = QString::fromUtf8(schemaFile.readAll());
    const QStringList statements = schema.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QSqlQuery query(database_);
    for (const QString &rawStatement : statements) {
        const QString statement = rawStatement.trimmed();
        if (statement.isEmpty()) {
            continue;
        }
        if (!execute(query, statement, errorMessage)) {
            return false;
        }
    }

    QSqlQuery versionQuery(database_);
    versionQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO schema_version(version, applied_at) VALUES(1, ?)"));
    versionQuery.addBindValue(utcNow());
    if (!versionQuery.exec()) {
        if (errorMessage) {
            *errorMessage = versionQuery.lastError().text();
        }
        return false;
    }
    return true;
}

bool Database::migrateSchema(QString *errorMessage)
{
    // 增量迁移先检查列是否存在，使旧版数据库可以安全升级。
    auto ensureColumn = [this, errorMessage](const QString &table,
                                              const QString &column,
                                              const QString &definition) {
        QSqlQuery columns(database_);
        if (!columns.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
            if (errorMessage) *errorMessage = columns.lastError().text();
            return false;
        }
        while (columns.next()) {
            if (columns.value(1).toString() == column) return true;
        }
        QSqlQuery alter(database_);
        const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                                .arg(table, column, definition);
        if (!alter.exec(sql)) {
            if (errorMessage) *errorMessage = alter.lastError().text() + QStringLiteral(" | SQL: ") + sql;
            return false;
        }
        return true;
    };

    if (!ensureColumn(QStringLiteral("users"), QStringLiteral("avatar_mime"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !ensureColumn(QStringLiteral("users"), QStringLiteral("avatar_data"),
                         QStringLiteral("BLOB"))
        || !ensureColumn(QStringLiteral("users"), QStringLiteral("avatar_path"),
                         QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !ensureColumn(QStringLiteral("chargers"), QStringLiteral("total_charge_count"),
                         QStringLiteral("INTEGER NOT NULL DEFAULT 0"))
        || !ensureColumn(QStringLiteral("chargers"), QStringLiteral("total_duration_seconds"),
                         QStringLiteral("INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }

    bool hasLifecycleOrders = false;
    QSqlQuery orderColumns(database_);
    if (!orderColumns.exec(QStringLiteral("PRAGMA table_info(orders)"))) {
        if (errorMessage) *errorMessage = orderColumns.lastError().text();
        return false;
    }
    while (orderColumns.next()) {
        if (orderColumns.value(1).toString() == QStringLiteral("price_cents_per_kwh")) {
            hasLifecycleOrders = true;
            break;
        }
    }
    if (!hasLifecycleOrders) {
        QSqlQuery migrate(database_);
        const QStringList statements{
            QStringLiteral("ALTER TABLE orders RENAME TO orders_legacy_v2"),
            QStringLiteral(
                "CREATE TABLE orders ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, order_no TEXT NOT NULL UNIQUE, "
                "charging_session_id INTEGER UNIQUE REFERENCES charging_sessions(id), "
                "reservation_id INTEGER UNIQUE REFERENCES reservations(id), "
                "user_id INTEGER NOT NULL REFERENCES users(id), station_id INTEGER NOT NULL REFERENCES stations(id), "
                "charger_id INTEGER NOT NULL REFERENCES chargers(id), energy_wh INTEGER NOT NULL DEFAULT 0 CHECK(energy_wh >= 0), "
                "price_cents_per_kwh INTEGER NOT NULL CHECK(price_cents_per_kwh >= 0), "
                "amount_cents INTEGER NOT NULL DEFAULT 0 CHECK(amount_cents >= 0), "
                "status TEXT NOT NULL DEFAULT 'reserved' CHECK(status IN "
                "('reserved','charging','pending_settlement','settled','cancelled')), "
                "reserved_at TEXT NOT NULL, started_at TEXT, ended_at TEXT, settled_at TEXT, "
                "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
            QStringLiteral(
                "INSERT INTO orders(id, order_no, charging_session_id, user_id, station_id, charger_id, "
                "energy_wh, price_cents_per_kwh, amount_cents, status, reserved_at, started_at, ended_at, "
                "settled_at, created_at, updated_at) "
                "SELECT o.id, o.order_no, o.charging_session_id, o.user_id, o.station_id, o.charger_id, "
                "o.energy_wh, cs.price_cents_per_kwh, o.amount_cents, "
                "CASE o.status WHEN 'paid' THEN 'settled' WHEN 'pending' THEN 'pending_settlement' ELSE 'cancelled' END, "
                "o.created_at, cs.started_at, cs.ended_at, o.paid_at, o.created_at, o.created_at "
                "FROM orders_legacy_v2 o JOIN charging_sessions cs ON cs.id = o.charging_session_id"),
            QStringLiteral("DROP TABLE orders_legacy_v2"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_user_created ON orders(user_id, created_at DESC)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_user_status ON orders(user_id, status)")
        };
        for (const QString &statement : statements) {
            if (!migrate.exec(statement)) {
                if (errorMessage) *errorMessage = migrate.lastError().text() + QStringLiteral(" | SQL: ") + statement;
                return false;
            }
        }
    }

    QSqlQuery rebuildCounters(database_);
    if (!rebuildCounters.exec(QStringLiteral(
            "UPDATE chargers SET "
            "total_charge_count = (SELECT COUNT(*) FROM charging_sessions cs "
            "WHERE cs.charger_id = chargers.id AND cs.status = 'finished'), "
            "total_duration_seconds = COALESCE((SELECT SUM(MAX(0, CAST((julianday(cs.ended_at) - "
            "julianday(cs.started_at)) * 86400 AS INTEGER))) FROM charging_sessions cs "
            "WHERE cs.charger_id = chargers.id AND cs.status = 'finished'), 0)"))) {
        if (errorMessage) *errorMessage = rebuildCounters.lastError().text();
        return false;
    }

    QSqlQuery versionQuery(database_);
    versionQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO schema_version(version, applied_at) VALUES(2, ?)"));
    versionQuery.addBindValue(utcNow());
    if (!versionQuery.exec()) {
        if (errorMessage) *errorMessage = versionQuery.lastError().text();
        return false;
    }
    versionQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO schema_version(version, applied_at) VALUES(3, ?)"));
    versionQuery.addBindValue(utcNow());
    if (!versionQuery.exec()) {
        if (errorMessage) *errorMessage = versionQuery.lastError().text();
        return false;
    }
    versionQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO schema_version(version, applied_at) VALUES(4, ?)"));
    versionQuery.addBindValue(utcNow());
    if (!versionQuery.exec()) {
        if (errorMessage) *errorMessage = versionQuery.lastError().text();
        return false;
    }
    return true;
}

bool Database::seedDefaults(QString *errorMessage)
{
    // 默认账号、价格、站点和充电桩在一个事务内初始化，失败则整体回滚。
    if (!database_.transaction()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    auto rollbackWithError = [this, errorMessage](const QString &message) {
        database_.rollback();
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    QSqlQuery query(database_);
    const QString now = utcNow();
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO tariffs(id, name, price_cents_per_kwh, active, created_at, updated_at) "
        "VALUES(1, '标准充电', 80, 1, ?, ?)"));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        return rollbackWithError(query.lastError().text());
    }

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO tariffs(id, name, price_cents_per_kwh, active, created_at, updated_at) "
        "VALUES(2, '快速充电', 120, 1, ?, ?)"));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        return rollbackWithError(query.lastError().text());
    }

    struct StationSeed {
        int id;
        const char *name;
        const char *region;
        const char *address;
        double longitude;
        double latitude;
    };
    const StationSeed stations[] = {
        {1, "中关村充电中心", "海淀区", "北京市海淀区中关村南大街 5 号", 116.3269, 39.9573},
        {2, "亦庄新能源站", "大兴区", "北京市大兴区荣华中路 10 号", 116.5068, 39.7942},
        {3, "望京科技园站", "朝阳区", "北京市朝阳区望京东路 1 号", 116.4878, 39.9979}
    };
    for (const StationSeed &station : stations) {
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO stations(id, name, region, address, longitude, latitude, "
            "business_hours, status, created_at, updated_at) "
            "VALUES(?, ?, ?, ?, ?, ?, '00:00-24:00', 'active', ?, ?)"));
        query.addBindValue(station.id);
        query.addBindValue(QString::fromUtf8(station.name));
        query.addBindValue(QString::fromUtf8(station.region));
        query.addBindValue(QString::fromUtf8(station.address));
        query.addBindValue(station.longitude);
        query.addBindValue(station.latitude);
        query.addBindValue(now);
        query.addBindValue(now);
        if (!query.exec()) {
            return rollbackWithError(query.lastError().text());
        }
    }

    struct ChargerSeed {
        int id;
        int stationId;
        const char *code;
        double powerKw;
        int tariffId;
    };
    const ChargerSeed chargers[] = {
        {1, 1, "HD-ZGC-001", 7.0, 1},
        {2, 1, "HD-ZGC-002", 60.0, 2},
        {3, 2, "DX-YZ-001", 60.0, 2},
        {4, 2, "DX-YZ-002", 7.0, 1},
        {5, 3, "CY-WJ-001", 7.0, 1},
        {6, 3, "CY-WJ-002", 60.0, 2}
    };
    for (const ChargerSeed &charger : chargers) {
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO chargers(id, station_id, code, connector_type, rated_power_kw, "
            "status, tariff_id, created_at, updated_at) "
            "VALUES(?, ?, ?, 'GB/T', ?, 'idle', ?, ?, ?)"));
        query.addBindValue(charger.id);
        query.addBindValue(charger.stationId);
        query.addBindValue(QString::fromLatin1(charger.code));
        query.addBindValue(charger.powerKw);
        query.addBindValue(charger.tariffId);
        query.addBindValue(now);
        query.addBindValue(now);
        if (!query.exec()) {
            return rollbackWithError(query.lastError().text());
        }
    }

    if (!seedUser(QStringLiteral("admin"), QStringLiteral("123456"),
                  QStringLiteral("admin"), QStringLiteral("运营管理员"), 0,
                  errorMessage)) {
        database_.rollback();
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO admins(user_id, username, password_hash, password_salt, display_name, "
        "status, created_at, updated_at) "
        "SELECT id, username, password_hash, password_salt, display_name, status, created_at, updated_at "
        "FROM users WHERE username = 'admin' AND role = 'admin'"));
    if (!query.exec()) {
        return rollbackWithError(query.lastError().text());
    }
    if (!seedUser(QStringLiteral("demo"), QStringLiteral("Demo123!"),
                  QStringLiteral("user"), QStringLiteral("演示用户"), 20000,
                  errorMessage)) {
        database_.rollback();
        return false;
    }
    query.prepare(QStringLiteral(
        "UPDATE users SET phone = '13800138000', updated_at = ? "
        "WHERE username = 'demo' AND phone = '' "
        "AND NOT EXISTS (SELECT 1 FROM users u2 WHERE u2.phone = '13800138000')"));
    query.addBindValue(now);
    if (!query.exec()) {
        return rollbackWithError(query.lastError().text());
    }

    if (!database_.commit()) {
        return rollbackWithError(database_.lastError().text());
    }
    return true;
}

bool Database::seedUser(const QString &username,
                        const QString &password,
                        const QString &role,
                        const QString &displayName,
                        qint64 balanceCents,
                        QString *errorMessage)
{
    QSqlQuery exists(database_);
    exists.prepare(QStringLiteral("SELECT 1 FROM users WHERE username = ?"));
    exists.addBindValue(username);
    if (!exists.exec()) {
        if (errorMessage) {
            *errorMessage = exists.lastError().text();
        }
        return false;
    }
    if (exists.next()) {
        return true;
    }

    const QString salt = security::createSalt();
    const QString now = utcNow();
    QSqlQuery insert(database_);
    insert.prepare(QStringLiteral(
        "INSERT INTO users(username, password_hash, password_salt, role, display_name, "
        "balance_cents, status, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, 'active', ?, ?)"));
    insert.addBindValue(username);
    insert.addBindValue(security::hashPassword(password, salt));
    insert.addBindValue(salt);
    insert.addBindValue(role);
    insert.addBindValue(displayName);
    insert.addBindValue(balanceCents);
    insert.addBindValue(now);
    insert.addBindValue(now);
    if (!insert.exec()) {
        if (errorMessage) {
            *errorMessage = insert.lastError().text();
        }
        return false;
    }
    return true;
}

} // namespace evcs::server
