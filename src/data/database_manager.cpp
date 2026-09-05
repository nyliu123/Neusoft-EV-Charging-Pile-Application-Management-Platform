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

namespace {

QStringList sqlStatements(const QString &script)
{
    QString withoutComments;
    const QStringList lines = script.split('\n');
    for (const QString &line : lines) {
        if (!line.trimmed().startsWith(QStringLiteral("--"))) {
            withoutComments.append(line);
            withoutComments.append('\n');
        }
    }

    QStringList statements;
    for (const QString &part : withoutComments.split(';')) {
        if (!part.trimmed().isEmpty()) {
            statements.append(part.trimmed());
        }
    }
    return statements;
}

const QStringList migrationScripts()
{
    return {
        QStringLiteral(":/database/migrations/001_core.sql"),
        QStringLiteral(":/database/migrations/002_seed_admin.sql"),
        QStringLiteral(":/database/migrations/003_seed_test_data.sql")
    };
}

} // namespace

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
    int executed = 0;

    for (const QString &scriptPath : migrationScripts()) {
        QFile migration(scriptPath);
        if (!migration.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return Result<int>::fail(ErrorCode::StorageError,
                QStringLiteral("migration script not found: %1").arg(scriptPath));
        }

        if (!database.transaction()) {
            return Result<int>::fail(ErrorCode::StorageError, database.lastError().text());
        }

        const QString script = QString::fromUtf8(migration.readAll());
        for (const QString &statement : sqlStatements(script)) {
            if (!query.exec(statement)) {
                database.rollback();
                return Result<int>::fail(ErrorCode::StorageError,
                    QStringLiteral("%1: %2").arg(scriptPath, query.lastError().text()));
            }
            ++executed;
        }

        if (!database.commit()) {
            database.rollback();
            return Result<int>::fail(ErrorCode::StorageError, database.lastError().text());
        }
    }

    return Result<int>::ok(executed);
}

} // namespace ev
