#include "services/comment_service.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSqlError>

namespace ev {

namespace {

constexpr int kMaxContentLength = 200;

QString nowText()
{
    return QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
}

} // namespace

QString CommentService::tierLabel(double avgRating)
{
    if (avgRating >= 9.0) {
        return QStringLiteral("夯");
    }
    if (avgRating >= 7.0) {
        return QStringLiteral("顶级");
    }
    if (avgRating >= 5.0) {
        return QStringLiteral("人上人");
    }
    if (avgRating >= 3.0) {
        return QStringLiteral("拉");
    }
    return QStringLiteral("拉完了");
}

QJsonObject CommentService::summaryJson(const StationRatingSummary &summary)
{
    if (summary.ratingCount <= 0) {
        return QJsonObject {
            {QStringLiteral("avg"), QJsonValue::Null},
            {QStringLiteral("count"), 0},
            {QStringLiteral("tier"), QJsonValue::Null},
            {QStringLiteral("hot"), QJsonValue::Null}
        };
    }
    const double avg = qRound(summary.avgRating * 10.0) / 10.0;
    QJsonObject hot;
    if (summary.hotComment.has_value()) {
        hot = QJsonObject {
            {QStringLiteral("nickname"), summary.hotComment->displayName},
            {QStringLiteral("content"), summary.hotComment->content}
        };
    }
    return QJsonObject {
        {QStringLiteral("avg"), avg},
        {QStringLiteral("count"), summary.ratingCount},
        {QStringLiteral("tier"), tierLabel(avg)},
        {QStringLiteral("hot"), hot}
    };
}

Result<QJsonObject> CommentService::listComments(
    QSqlDatabase &database, qint64 stationId, qint64 viewerId) const
{
    if (stationId <= 0) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("无效的站点编号"));
    }
    const auto exists = repository_.stationExists(database, stationId);
    if (!exists.success) {
        return Result<QJsonObject>::fail(exists.code, exists.message);
    }
    if (!exists.data) {
        return Result<QJsonObject>::fail(ErrorCode::NotFound,
                                         QStringLiteral("充电站不存在"));
    }

    const auto summary = repository_.findSummary(database, stationId);
    if (!summary.success) {
        return Result<QJsonObject>::fail(summary.code, summary.message);
    }
    const auto comments = repository_.listByStation(database, stationId, viewerId);
    if (!comments.success) {
        return Result<QJsonObject>::fail(comments.code, comments.message);
    }

    QJsonArray array;
    for (const CommentRecord &comment : comments.data) {
        array.append(QJsonObject {
            {QStringLiteral("comment_id"), comment.commentId},
            {QStringLiteral("nickname"), comment.displayName},
            {QStringLiteral("content"), comment.content},
            {QStringLiteral("rating"), comment.rating},
            {QStringLiteral("like_count"), comment.likeCount},
            {QStringLiteral("liked_by_me"), comment.likedByViewer},
            {QStringLiteral("is_mine"), comment.isMine},
            {QStringLiteral("created_at"), comment.createdAt}
        });
    }
    return Result<QJsonObject>::ok(QJsonObject {
        {QStringLiteral("summary"),
         summary.data.has_value() ? summaryJson(summary.data.value())
                                  : summaryJson(StationRatingSummary{})},
        {QStringLiteral("comments"), array}
    });
}

Result<QJsonObject> CommentService::postComment(
    QSqlDatabase &database, qint64 userId, qint64 stationId,
    const QString &content, int rating) const
{
    const QString trimmed = content.trimmed();
    if (stationId <= 0) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("无效的站点编号"));
    }
    if (trimmed.isEmpty() || trimmed.size() > kMaxContentLength) {
        return Result<QJsonObject>::fail(
            ErrorCode::InvalidInput,
            QStringLiteral("评论内容需为 1~%1 字").arg(kMaxContentLength));
    }
    if (rating < 0 || rating > 5) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("评分需为 0~5 星"));
    }
    const auto exists = repository_.stationExists(database, stationId);
    if (!exists.success) {
        return Result<QJsonObject>::fail(exists.code, exists.message);
    }
    if (!exists.data) {
        return Result<QJsonObject>::fail(ErrorCode::NotFound,
                                         QStringLiteral("充电站不存在"));
    }

    if (!database.transaction()) {
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         QStringLiteral("cannot begin transaction"));
    }
    const auto own = repository_.findOwn(database, stationId, userId);
    if (!own.success) {
        database.rollback();
        return Result<QJsonObject>::fail(own.code, own.message);
    }
    if (own.data.has_value()) {
        const auto updated = repository_.update(
            database, own.data->commentId, trimmed, rating, nowText());
        if (!updated.success) {
            database.rollback();
            return Result<QJsonObject>::fail(updated.code, updated.message);
        }
        if (!database.commit()) {
            database.rollback();
            return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                             database.lastError().text());
        }
        return Result<QJsonObject>::ok(QJsonObject {
            {QStringLiteral("comment_id"), own.data->commentId},
            {QStringLiteral("updated"), true}
        });
    }
    const auto inserted = repository_.insert(
        database, stationId, userId, trimmed, rating, nowText());
    if (!inserted.success) {
        database.rollback();
        return Result<QJsonObject>::fail(inserted.code, inserted.message);
    }
    if (!database.commit()) {
        database.rollback();
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         database.lastError().text());
    }
    return Result<QJsonObject>::ok(QJsonObject {
        {QStringLiteral("comment_id"), inserted.data},
        {QStringLiteral("updated"), false}
    });
}

Result<QJsonObject> CommentService::toggleLike(
    QSqlDatabase &database, qint64 userId, qint64 commentId) const
{
    if (commentId <= 0) {
        return Result<QJsonObject>::fail(ErrorCode::InvalidInput,
                                         QStringLiteral("无效的评论编号"));
    }
    const auto exists = repository_.commentExists(database, commentId);
    if (!exists.success) {
        return Result<QJsonObject>::fail(exists.code, exists.message);
    }
    if (!exists.data) {
        return Result<QJsonObject>::fail(ErrorCode::NotFound,
                                         QStringLiteral("评论不存在"));
    }

    if (!database.transaction()) {
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         QStringLiteral("cannot begin transaction"));
    }
    const auto outcome = repository_.toggleLike(database, commentId, userId,
                                                nowText());
    if (!outcome.success) {
        database.rollback();
        return Result<QJsonObject>::fail(outcome.code, outcome.message);
    }
    if (!database.commit()) {
        database.rollback();
        return Result<QJsonObject>::fail(ErrorCode::StorageError,
                                         database.lastError().text());
    }
    return Result<QJsonObject>::ok(QJsonObject {
        {QStringLiteral("liked"), outcome.data.first},
        {QStringLiteral("like_count"), outcome.data.second}
    });
}

Result<QHash<qint64, StationRatingSummary>> CommentService::loadSummaries(
    QSqlDatabase &database) const
{
    return repository_.loadSummaries(database);
}

Result<std::optional<StationRatingSummary>> CommentService::stationSummary(
    QSqlDatabase &database, qint64 stationId) const
{
    return repository_.findSummary(database, stationId);
}

} // namespace ev
