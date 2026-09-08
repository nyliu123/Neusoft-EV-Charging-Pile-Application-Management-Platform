#include "knowledge_service.h"
#include "membership_service.h"
#include "sql_support.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>

namespace ev {
using namespace sqlsupport;
Result<QJsonObject> KnowledgeService::list(QSqlDatabase &db) const {
    return guard(db,false,[&] { QJsonArray articles;
        auto q = query(db,"SELECT * FROM knowledge_articles ORDER BY article_id DESC LIMIT 500");
        while(q.next()) articles.append(row(q));
        return JsonResult::ok({{"articles",articles}});
    });
}
Result<QJsonObject> KnowledgeService::save(QSqlDatabase &db,const QJsonObject &p) const {
    if (!integer(p,"article_id",0,1000000000) || !integer(p,"draft_version",0,1000000000))
        return JsonResult::fail(ErrorCode::InvalidInput,QStringLiteral("知识编号或版本无效"));
    for (const char *key : {"title","keywords","content","source"}) {
        const QString value=p.value(QLatin1String(key)).toString().trimmed();
        const int max = QString(key)=="content" ? 12000 : QString(key)=="title" ? 120 : 500;
        if (value.isEmpty() || value.size()>max) return JsonResult::fail(ErrorCode::InvalidInput,QStringLiteral("标题、关键词、正文和来源必填，正文最多12000字"));
    }
    const auto keywords=p.value("keywords").toString().split(QRegularExpression("[,，;；\\s]+"),Qt::SkipEmptyParts);
    if(std::none_of(keywords.begin(),keywords.end(),[](const QString &word){return word.size()>=2;}))
        return JsonResult::fail(ErrorCode::InvalidInput,QStringLiteral("至少提供一个长度不小于2字的检索关键词"));
    return guard(db,true,[&] {
        qint64 id=p.value("article_id").toInteger();
        QVariantList values{p.value("title").toString().trimmed(),p.value("keywords").toString().trimmed(),p.value("content").toString().trimmed(),p.value("source").toString().trimmed(),MembershipService::now()};
        if (!id) {
            auto count=query(db,"SELECT COUNT(*) FROM knowledge_articles");count.next();
            if(count.value(0).toInt()>=500)return JsonResult::fail(ErrorCode::InvalidInput,QStringLiteral("教学知识库最多500条，请编辑已有知识"));
            auto q=query(db,"INSERT INTO knowledge_articles(title,keywords,content,source,updated_at) VALUES(?,?,?,?,?)",values); id=q.lastInsertId().toLongLong();
        } else {
            values.append(id); values.append(p.value("draft_version").toInt());
            auto q=query(db,"UPDATE knowledge_articles SET title=?,keywords=?,content=?,source=?,updated_at=?,draft_version=draft_version+1 WHERE article_id=? AND draft_version=?",values);
            if(q.numRowsAffected()!=1) return JsonResult::fail(ErrorCode::StateConflict,QStringLiteral("知识已变更，请刷新；原发布版本保持有效"));
        }
        return JsonResult::ok({{"article_id",id}});
    });
}
Result<QJsonObject> KnowledgeService::publish(QSqlDatabase &db,const QJsonObject &p,bool active) const {
    if (!integer(p,"article_id",1,1000000000) || !integer(p,"draft_version",1,1000000000)) return JsonResult::fail(ErrorCode::InvalidInput,QStringLiteral("知识编号或版本无效"));
    return guard(db,true,[&] {
        const QString sql=active
            ? "UPDATE knowledge_articles SET published_title=title,published_keywords=keywords,published_content=content,published_source=source,published_version=draft_version,active=1,updated_at=? WHERE article_id=? AND draft_version=? AND length(trim(content))>0 AND length(trim(source))>0"
            : "UPDATE knowledge_articles SET active=0,updated_at=? WHERE article_id=? AND draft_version=?";
        auto q=query(db,sql,{MembershipService::now(),p.value("article_id").toInteger(),p.value("draft_version").toInt()});
        if(q.numRowsAffected()!=1) return JsonResult::fail(ErrorCode::StateConflict,QStringLiteral("知识版本已变化，操作未生效，请刷新"));
        return JsonResult::ok({{"published",active}});
    });
}
Result<QJsonObject> KnowledgeService::retrieve(QSqlDatabase &db,const QString &question) const {
    return guard(db,false,[&] {
        QList<QPair<int,QJsonObject>> matches;
        auto q=query(db,"SELECT article_id,published_title AS title,published_keywords AS keywords,published_content AS content,published_source AS source,published_version AS version FROM knowledge_articles WHERE active=1 AND published_version>0 LIMIT 500");
        const QString normalized=question.toLower();
        while(q.next()) {
            auto a=row(q); int score=0;
            const auto keywords=a.value("keywords").toString().toLower().split(QRegularExpression("[,，;；\\s]+"),Qt::SkipEmptyParts);
            for(const auto &word:keywords) if(word.size()>=2 && normalized.contains(word)) score+=word.size();
            if(score) matches.append({score,a});
        }
        std::stable_sort(matches.begin(),matches.end(),[](const auto &a,const auto &b){ return a.first>b.first; });
        QJsonArray sources;
        int remaining=14000;
        for(int i=0;i<qMin(4,matches.size()) && remaining>0;++i) {
            auto a=matches[i].second;
            const QString content=a.value("content").toString().left(remaining);
            remaining-=content.size(); a.insert("content",content); a.remove("keywords"); sources.append(a);
        }
        return JsonResult::ok({{"sources",sources}});
    });
}
}
