#include "services/session_manager.h"

#include <QDateTime>
#include <QMutexLocker>

namespace ev {

SessionManager::SessionManager(qint64 timeoutMs)
    : timeoutMs_(timeoutMs)
{
}

bool SessionManager::registerUserSession(const QString &sessionId, qint64 userId)
{
    if (sessionId.isEmpty() || userId <= 0) {
        return false;
    }
    const QMutexLocker locker(&mutex_);
    sessions_.insert(sessionId, {userId, QDateTime::currentMSecsSinceEpoch()});
    return true;
}

bool SessionManager::validateAndTouch(const QString &sessionId)
{
    if (sessionId.isEmpty()) {
        return false;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QMutexLocker locker(&mutex_);
    auto session = sessions_.find(sessionId);
    if (session == sessions_.end()) {
        return false;
    }
    if (timeoutMs_ >= 0 && now - session->lastSeenMs > timeoutMs_) {
        sessions_.erase(session);
        return false;
    }
    session->lastSeenMs = now;
    return true;
}

bool SessionManager::remove(const QString &sessionId)
{
    const QMutexLocker locker(&mutex_);
    return sessions_.remove(sessionId) > 0;
}

int SessionManager::removeAll(const QSet<QString> &sessionIds)
{
    const QMutexLocker locker(&mutex_);
    int removed = 0;
    for (const QString &sessionId : sessionIds) {
        removed += sessions_.remove(sessionId);
    }
    return removed;
}

int SessionManager::removeByUserId(qint64 userId)
{
    const QMutexLocker locker(&mutex_);
    int removed = 0;
    for (auto session = sessions_.begin(); session != sessions_.end();) {
        if (session->userId == userId) {
            session = sessions_.erase(session);
            ++removed;
        } else {
            ++session;
        }
    }
    return removed;
}

int SessionManager::activeSessionCount() const
{
    const QMutexLocker locker(&mutex_);
    return sessions_.size();
}

} // namespace ev
