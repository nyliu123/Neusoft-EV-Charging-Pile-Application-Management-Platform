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

    return executeSchema(schemaPath, errorMessage) && seedDefaults(errorMessage);
}

QSqlDatabase Database::connection() const
{
    return database_;
}

bool Database::executeSchema(const QString &schemaPath, QString *errorMessage)
{
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

bool Database::seedDefaults(QString *errorMessage)
{
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

    if (!seedUser(QStringLiteral("admin"), QStringLiteral("Admin123!"),
                  QStringLiteral("admin"), QStringLiteral("运营管理员"), 0,
                  errorMessage)) {
        database_.rollback();
        return false;
    }
    if (!seedUser(QStringLiteral("demo"), QStringLiteral("Demo123!"),
                  QStringLiteral("user"), QStringLiteral("演示用户"), 20000,
                  errorMessage)) {
        database_.rollback();
        return false;
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
