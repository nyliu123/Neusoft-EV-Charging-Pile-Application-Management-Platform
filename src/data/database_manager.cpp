#include "data/database_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <utility>

void initializeDatabaseResources()
{
    Q_INIT_RESOURCE(database);
}

namespace ev {

DatabaseManager::DatabaseManager(QString databasePath, int busyTimeoutMs)
    : databasePath_(std::move(databasePath)), busyTimeoutMs_(busyTimeoutMs)
{
    initializeDatabaseResources();
}

QString DatabaseManager::connectionName() const
{
    return QStringLiteral("ev-db-%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

Result<QSqlDatabase> DatabaseManager::openForCurrentThread() const
{
    const QFileInfo databaseFile(databasePath_);
    if (!QDir().mkpath(databaseFile.absolutePath())) {
        return Result<QSqlDatabase>::fail(ErrorCode::StorageError,
                                          QStringLiteral("cannot create database directory"));
    }
    if (!databaseFile.exists()) {
        if (!QFile::copy(QStringLiteral(":/database/template/ev_charging.sqlite3"),
                         databaseFile.absoluteFilePath())) {
            return Result<QSqlDatabase>::fail(
                ErrorCode::StorageError,
                QStringLiteral("cannot create database from bundled template"));
        }
        QFile::setPermissions(databaseFile.absoluteFilePath(),
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    const QString name = connectionName();
    QSqlDatabase database = QSqlDatabase::contains(name)
        ? QSqlDatabase::database(name)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(databaseFile.absoluteFilePath());
    if (!database.open()) {
        return Result<QSqlDatabase>::fail(ErrorCode::StorageError,
                                          database.lastError().text());
    }

    QSqlQuery query(database);
    // WAL needs mmap-coherent shared memory, which VMware shared folders
    // (hgfs) do not provide: a running server keeps a stale read snapshot
    // and never sees writes from other processes (e.g. seed scripts).
    // DELETE journal mode keeps per-query read visibility correct there.
    // The switch also checkpoints and converts databases already flagged WAL.
    const QStringList pragmas {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA busy_timeout = %1").arg(busyTimeoutMs_),
        QStringLiteral("PRAGMA journal_mode = DELETE")
    };
    for (const QString &statement : pragmas) {
        if (!query.exec(statement)) {
            return Result<QSqlDatabase>::fail(ErrorCode::StorageError,
                                              query.lastError().text());
        }
    }
    return Result<QSqlDatabase>::ok(database);
}

Result<int> DatabaseManager::migrate(QSqlDatabase &database) const
{
    QSqlQuery query(database);
    const QStringList requiredTables {
        QStringLiteral("users"), QStringLiteral("admins"),
        QStringLiteral("charging_stations"), QStringLiteral("charging_piles"),
        QStringLiteral("orders")
    };
    for (const QString &table : requiredTables) {
        query.prepare(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
        query.addBindValue(table);
        if (!query.exec() || !query.next()) {
            return Result<int>::fail(
                ErrorCode::StorageError,
                QStringLiteral("database template is missing table: %1").arg(table));
        }
    }

    // Station comments and likes: created idempotently so runtime databases
    // copied from the bundled template gain the tables on first start.
    const QStringList commentSchemas {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS station_comments ("
            "comment_id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "station_id INTEGER NOT NULL REFERENCES charging_stations(station_id), "
            "user_id INTEGER NOT NULL REFERENCES users(user_id), "
            "content TEXT NOT NULL, "
            "rating INTEGER NOT NULL, "
            "created_at TEXT NOT NULL, "
            "updated_at TEXT NOT NULL, "
            "UNIQUE (station_id, user_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS comment_likes ("
            "like_id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "comment_id INTEGER NOT NULL REFERENCES station_comments(comment_id), "
            "user_id INTEGER NOT NULL REFERENCES users(user_id), "
            "created_at TEXT NOT NULL, "
            "UNIQUE (comment_id, user_id))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_station_comments_station "
            "ON station_comments(station_id)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_comment_likes_comment "
            "ON comment_likes(comment_id)")
    };
    for (const QString &statement : commentSchemas) {
        if (!query.exec(statement)) {
            return Result<int>::fail(
                ErrorCode::StorageError,
                QStringLiteral("cannot create comment tables: %1")
                    .arg(query.lastError().text()));
        }
    }
    return Result<int>::ok(0);
}

// ── UML-053: transaction wrapper ─────────────────────────────────────────

Result<bool> DatabaseManager::executeTransaction(
    QSqlDatabase &database, const std::function<bool()> &fn) const
{
    if (!database.transaction()) {
        return Result<bool>::fail(ErrorCode::StorageError,
                                   database.lastError().text());
    }
    try {
        if (!fn()) {
            database.rollback();
            return Result<bool>::fail(ErrorCode::InternalError,
                                       QStringLiteral("transaction_callback_failed"));
        }
        if (!database.commit()) {
            database.rollback();
            return Result<bool>::fail(ErrorCode::StorageError,
                                       database.lastError().text());
        }
        return Result<bool>::ok(true);
    } catch (...) {
        database.rollback();
        return Result<bool>::fail(ErrorCode::StorageError,
                                   QStringLiteral("transaction_exception"));
    }
}

} // namespace ev
