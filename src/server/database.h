#pragma once

#include <QSqlDatabase>
#include <QString>

namespace evcs::server {

class Database final
{
public:
    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool initialize(const QString &databasePath,
                    const QString &schemaPath,
                    QString *errorMessage = nullptr);
    bool openExisting(const QString &databasePath,
                      QString *errorMessage = nullptr);

    QSqlDatabase connection() const;

private:
    bool executeSchema(const QString &schemaPath, QString *errorMessage);
    bool migrateSchema(QString *errorMessage);
    bool seedDefaults(QString *errorMessage);
    bool seedUser(const QString &username,
                  const QString &password,
                  const QString &role,
                  const QString &displayName,
                  qint64 balanceCents,
                  QString *errorMessage);

    QString connectionName_;
    QSqlDatabase database_;
};

} // namespace evcs::server
