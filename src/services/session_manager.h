#pragma once

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>

namespace ev {

class SessionManager final {
public:
    explicit SessionManager(qint64 timeoutMs = 120000);

    bool registerUserSession(const QString &sessionId, qint64 userId);
    bool validateAndTouch(const QString &sessionId);
    qint64 authenticatedUserId(const QString &sessionId);
    bool remove(const QString &sessionId);
    int removeAll(const QSet<QString> &sessionIds);
    int removeByUserId(qint64 userId);
    int activeSessionCount() const;

private:
    struct Session {
        qint64 userId = 0;
        qint64 lastSeenMs = 0;
    };

    qint64 timeoutMs_;
    mutable QMutex mutex_;
    QHash<QString, Session> sessions_;
};

} // namespace ev
