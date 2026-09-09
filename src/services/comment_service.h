#pragma once

#include "common/result.h"
#include "data/comment_repository.h"

#include <QJsonObject>
#include <QSqlDatabase>

namespace ev {

class CommentService final {
public:
    // Tier label for a 0..5 average rating:
    // ≥4.5 夯 / ≥3.5 顶级 / ≥2.5 人上人 / ≥1.5 拉 / else 拉完了.
    static QString tierLabel(double avgRating);
    // Wire shape: { avg, count, tier, hot{nickname, content} }; avg/tier/hot
    // are null when the station has no comments.
    static QJsonObject summaryJson(const StationRatingSummary &summary);

    // All comments of a station (hottest first) plus the rating summary.
    Result<QJsonObject> listComments(QSqlDatabase &database, qint64 stationId,
                                     qint64 viewerId) const;
    // Insert or update the user's single comment for the station.
    Result<QJsonObject> postComment(QSqlDatabase &database, qint64 userId,
                                    qint64 stationId, const QString &content,
                                    int rating) const;
    Result<QJsonObject> toggleLike(QSqlDatabase &database, qint64 userId,
                                   qint64 commentId) const;
    Result<QHash<qint64, StationRatingSummary>> loadSummaries(
        QSqlDatabase &database) const;
    Result<std::optional<StationRatingSummary>> stationSummary(
        QSqlDatabase &database, qint64 stationId) const;

private:
    CommentRepository repository_;
};

} // namespace ev
