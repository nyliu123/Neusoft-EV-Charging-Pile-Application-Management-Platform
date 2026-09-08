#include "data/comment_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ev {

namespace {

// Display name: nickname when set, otherwise the phone number.
const char kDisplayName[] =
    "COALESCE(NULLIF(TRIM(u.nickname), ''), u.phone)";

const char kLikeCount[] =
    "(SELECT COUNT(*) FROM comment_likes l WHERE l.comment_id = c.comment_id)";

} // namespace

Result<QHash<qint64, StationRatingSummary>> CommentRepository::loadSummaries(
    QSqlDatabase &database) const
{
    QHash<qint64, StationRatingSummary> summaries;

    QSqlQuery aggregate(database);
    if (!aggregate.exec(QStringLiteral(
            "SELECT station_id, COUNT(*), AVG(rating) FROM station_comments "
            "GROUP BY station_id"))) {
        return Result<QHash<qint64, StationRatingSummary>>::fail(
            ErrorCode::StorageError, aggregate.lastError().text());
    }
    while (aggregate.next()) {
        StationRatingSummary summary;
        summary.ratingCount = aggregate.value(1).toInt();
        summary.avgRating = aggregate.value(2).toDouble() * 2.0;
        summaries.insert(aggregate.value(0).toLongLong(), summary);
    }

    // Rows are grouped by station and ordered by like count, so the first row
    // per station is the hot comment.
    QSqlQuery hot(database);
    if (!hot.exec(QStringLiteral(
            "SELECT c.station_id, c.comment_id, %1, c.content, c.rating, "
            "c.created_at, %2 AS like_count "
            "FROM station_comments c "
            "LEFT JOIN users u ON u.user_id = c.user_id "
            "ORDER BY c.station_id, like_count DESC, c.created_at ASC, "
            "c.comment_id ASC").arg(QLatin1String(kDisplayName),
                                    QLatin1String(kLikeCount)))) {
        return Result<QHash<qint64, StationRatingSummary>>::fail(
            ErrorCode::StorageError, hot.lastError().text());
    }
    while (hot.next()) {
        const qint64 stationId = hot.value(0).toLongLong();
        const auto found = summaries.find(stationId);
        if (found == summaries.end() || found->hotComment.has_value()) {
            continue;
        }
        CommentRecord comment;
        comment.stationId = stationId;
        comment.commentId = hot.value(1).toLongLong();
        comment.displayName = hot.value(2).toString();
        comment.content = hot.value(3).toString();
        comment.rating = hot.value(4).toInt();
        comment.createdAt = hot.value(5).toString();
        comment.likeCount = hot.value(6).toInt();
        found->hotComment = comment;
    }
    return Result<QHash<qint64, StationRatingSummary>>::ok(summaries);
}

Result<std::optional<StationRatingSummary>> CommentRepository::findSummary(
    QSqlDatabase &database, qint64 stationId) const
{
    QSqlQuery aggregate(database);
    aggregate.prepare(QStringLiteral(
        "SELECT COUNT(*), AVG(rating) FROM station_comments WHERE station_id = ?"));
    aggregate.addBindValue(stationId);
    if (!aggregate.exec()) {
        return Result<std::optional<StationRatingSummary>>::fail(
            ErrorCode::StorageError, aggregate.lastError().text());
    }
    if (!aggregate.next() || aggregate.value(0).toInt() <= 0) {
        return Result<std::optional<StationRatingSummary>>::ok(std::nullopt);
    }

    StationRatingSummary summary;
    summary.ratingCount = aggregate.value(0).toInt();
    summary.avgRating = aggregate.value(1).toDouble() * 2.0;

    QSqlQuery hot(database);
    hot.prepare(QStringLiteral(
        "SELECT c.comment_id, %1, c.content, c.rating, c.created_at, "
        "%2 AS like_count "
        "FROM station_comments c "
        "LEFT JOIN users u ON u.user_id = c.user_id "
        "WHERE c.station_id = ? "
        "ORDER BY like_count DESC, c.created_at ASC, c.comment_id ASC LIMIT 1")
                    .arg(QLatin1String(kDisplayName),
                         QLatin1String(kLikeCount)));
    hot.addBindValue(stationId);
    if (!hot.exec()) {
        return Result<std::optional<StationRatingSummary>>::fail(
            ErrorCode::StorageError, hot.lastError().text());
    }
    if (hot.next()) {
        CommentRecord comment;
        comment.stationId = stationId;
        comment.commentId = hot.value(0).toLongLong();
        comment.displayName = hot.value(1).toString();
        comment.content = hot.value(2).toString();
        comment.rating = hot.value(3).toInt();
        comment.createdAt = hot.value(4).toString();
        comment.likeCount = hot.value(5).toInt();
        summary.hotComment = comment;
    }
    return Result<std::optional<StationRatingSummary>>::ok(summary);
}

Result<QVector<CommentRecord>> CommentRepository::listByStation(
    QSqlDatabase &database, qint64 stationId, qint64 viewerId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT c.comment_id, c.user_id, %1, c.content, c.rating, c.created_at, "
        "%2 AS like_count, "
        "EXISTS(SELECT 1 FROM comment_likes m "
        "       WHERE m.comment_id = c.comment_id AND m.user_id = ?) "
        "FROM station_comments c "
        "LEFT JOIN users u ON u.user_id = c.user_id "
        "WHERE c.station_id = ? "
        "ORDER BY like_count DESC, c.created_at DESC, c.comment_id DESC")
                    .arg(QLatin1String(kDisplayName),
                         QLatin1String(kLikeCount)));
    query.addBindValue(viewerId);
    query.addBindValue(stationId);
    if (!query.exec()) {
        return Result<QVector<CommentRecord>>::fail(ErrorCode::StorageError,
                                                    query.lastError().text());
    }

    QVector<CommentRecord> comments;
    while (query.next()) {
        CommentRecord comment;
        comment.stationId = stationId;
        comment.commentId = query.value(0).toLongLong();
        comment.userId = query.value(1).toLongLong();
        comment.displayName = query.value(2).toString();
        comment.content = query.value(3).toString();
        comment.rating = query.value(4).toInt();
        comment.createdAt = query.value(5).toString();
        comment.likeCount = query.value(6).toInt();
        comment.likedByViewer = query.value(7).toBool();
        comment.isMine = comment.userId == viewerId;
        comments.append(comment);
    }
    return Result<QVector<CommentRecord>>::ok(comments);
}

Result<std::optional<CommentRecord>> CommentRepository::findOwn(
    QSqlDatabase &database, qint64 stationId, qint64 userId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT c.comment_id, c.user_id, %1, c.content, c.rating, c.created_at, "
        "%2 AS like_count, "
        "EXISTS(SELECT 1 FROM comment_likes m "
        "       WHERE m.comment_id = c.comment_id AND m.user_id = ?) "
        "FROM station_comments c "
        "LEFT JOIN users u ON u.user_id = c.user_id "
        "WHERE c.station_id = ? AND c.user_id = ?")
                    .arg(QLatin1String(kDisplayName),
                         QLatin1String(kLikeCount)));
    query.addBindValue(userId);
    query.addBindValue(stationId);
    query.addBindValue(userId);
    if (!query.exec()) {
        return Result<std::optional<CommentRecord>>::fail(
            ErrorCode::StorageError, query.lastError().text());
    }
    if (!query.next()) {
        return Result<std::optional<CommentRecord>>::ok(std::nullopt);
    }
    CommentRecord comment;
    comment.stationId = stationId;
    comment.commentId = query.value(0).toLongLong();
    comment.userId = query.value(1).toLongLong();
    comment.displayName = query.value(2).toString();
    comment.content = query.value(3).toString();
    comment.rating = query.value(4).toInt();
    comment.createdAt = query.value(5).toString();
    comment.likeCount = query.value(6).toInt();
    comment.likedByViewer = query.value(7).toBool();
    comment.isMine = true;
    return Result<std::optional<CommentRecord>>::ok(comment);
}

Result<bool> CommentRepository::stationExists(QSqlDatabase &database,
                                              qint64 stationId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM charging_stations WHERE station_id = ? LIMIT 1"));
    query.addBindValue(stationId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<bool>::ok(query.next());
}

Result<bool> CommentRepository::commentExists(QSqlDatabase &database,
                                              qint64 commentId) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM station_comments WHERE comment_id = ? LIMIT 1"));
    query.addBindValue(commentId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<bool>::ok(query.next());
}

Result<qint64> CommentRepository::insert(QSqlDatabase &database, qint64 stationId,
                                         qint64 userId, const QString &content,
                                         int rating, const QString &now) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO station_comments "
        "(station_id, user_id, content, rating, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(stationId);
    query.addBindValue(userId);
    query.addBindValue(content);
    query.addBindValue(rating);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        return Result<qint64>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<qint64>::ok(query.lastInsertId().toLongLong());
}

Result<bool> CommentRepository::update(QSqlDatabase &database, qint64 commentId,
                                       const QString &content, int rating,
                                       const QString &now) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE station_comments SET content = ?, rating = ?, updated_at = ? "
        "WHERE comment_id = ?"));
    query.addBindValue(content);
    query.addBindValue(rating);
    query.addBindValue(now);
    query.addBindValue(commentId);
    if (!query.exec()) {
        return Result<bool>::fail(ErrorCode::StorageError, query.lastError().text());
    }
    return Result<bool>::ok(true);
}

Result<QPair<bool, int>> CommentRepository::toggleLike(
    QSqlDatabase &database, qint64 commentId, qint64 userId,
    const QString &now) const
{
    QSqlQuery existing(database);
    existing.prepare(QStringLiteral(
        "SELECT 1 FROM comment_likes WHERE comment_id = ? AND user_id = ?"));
    existing.addBindValue(commentId);
    existing.addBindValue(userId);
    if (!existing.exec()) {
        return Result<QPair<bool, int>>::fail(ErrorCode::StorageError,
                                              existing.lastError().text());
    }
    const bool wasLiked = existing.next();

    QSqlQuery mutate(database);
    if (wasLiked) {
        mutate.prepare(QStringLiteral(
            "DELETE FROM comment_likes WHERE comment_id = ? AND user_id = ?"));
        mutate.addBindValue(commentId);
        mutate.addBindValue(userId);
    } else {
        mutate.prepare(QStringLiteral(
            "INSERT INTO comment_likes (comment_id, user_id, created_at) "
            "VALUES (?, ?, ?)"));
        mutate.addBindValue(commentId);
        mutate.addBindValue(userId);
        mutate.addBindValue(now);
    }
    if (!mutate.exec()) {
        return Result<QPair<bool, int>>::fail(ErrorCode::StorageError,
                                              mutate.lastError().text());
    }

    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM comment_likes WHERE comment_id = ?"));
    counter.addBindValue(commentId);
    if (!counter.exec() || !counter.next()) {
        return Result<QPair<bool, int>>::fail(ErrorCode::StorageError,
                                              counter.lastError().text());
    }
    return Result<QPair<bool, int>>::ok({!wasLiked, counter.value(0).toInt()});
}

} // namespace ev
