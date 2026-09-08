#pragma once

#include "common/result.h"

#include <QHash>
#include <QPair>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace ev {

struct CommentRecord {
    qint64 commentId = 0;
    qint64 stationId = 0;
    qint64 userId = 0;
    QString displayName;   // nickname, falling back to the phone number
    QString content;
    int rating = 0;        // 0..5 stars (= 0..10 points)
    int likeCount = 0;
    bool likedByViewer = false;
    bool isMine = false;
    QString createdAt;
};

struct StationRatingSummary {
    double avgRating = 0.0;   // 0..10 (stars × 2)
    int ratingCount = 0;
    std::optional<CommentRecord> hotComment;   // most liked comment
};

class CommentRepository final {
public:
    // Per-station aggregates for every station (station list page).
    Result<QHash<qint64, StationRatingSummary>> loadSummaries(
        QSqlDatabase &database) const;
    Result<std::optional<StationRatingSummary>> findSummary(
        QSqlDatabase &database, qint64 stationId) const;
    // All comments of one station, hottest first; viewer fields filled in.
    Result<QVector<CommentRecord>> listByStation(
        QSqlDatabase &database, qint64 stationId, qint64 viewerId) const;
    Result<std::optional<CommentRecord>> findOwn(
        QSqlDatabase &database, qint64 stationId, qint64 userId) const;
    Result<bool> stationExists(QSqlDatabase &database, qint64 stationId) const;
    Result<bool> commentExists(QSqlDatabase &database, qint64 commentId) const;
    Result<qint64> insert(QSqlDatabase &database, qint64 stationId, qint64 userId,
                          const QString &content, int rating,
                          const QString &now) const;
    Result<bool> update(QSqlDatabase &database, qint64 commentId,
                        const QString &content, int rating,
                        const QString &now) const;
    // Flips the user's like on a comment; returns (nowLiked, likeCount).
    Result<QPair<bool, int>> toggleLike(QSqlDatabase &database, qint64 commentId,
                                        qint64 userId, const QString &now) const;
};

} // namespace ev
