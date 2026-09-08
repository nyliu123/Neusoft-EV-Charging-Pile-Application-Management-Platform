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
    const QStringList pragmas {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA busy_timeout = %1").arg(busyTimeoutMs_)
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
