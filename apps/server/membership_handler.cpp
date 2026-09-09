#include "server_application.h"
#include "adapters/consult_api_adapter.h"
#include "common/protocol.h"
#include "services/membership_service.h"
#include "services/knowledge_service.h"
#include "services/user_service.h"
#include <QPointer>
#include <QJsonArray>
#include <QSqlQuery>
#include <QTcpSocket>

namespace ev {
namespace {
QString approvedSelect(const QString &tool,const QJsonObject &arguments)
{
    if(tool=="query_current_user")return QStringLiteral("SELECT user_id,phone,nickname,balance,register_time,status FROM users WHERE user_id=?");
    if(tool=="query_stations")return QStringLiteral("SELECT station_id,station_name,address,longitude,latitude,price_per_kwh FROM charging_stations WHERE ?='' OR station_name LIKE ? OR address LIKE ? ORDER BY station_id LIMIT ?");
    if(tool=="query_piles")return QStringLiteral("SELECT p.pile_id,p.station_id,s.station_name,p.pile_number,p.pile_type,p.power_kw,p.status,p.total_charge_count,p.total_charge_duration FROM charging_piles p JOIN charging_stations s ON s.station_id=p.station_id WHERE (?<=0 OR p.station_id=?) AND (?='' OR p.status=?) ORDER BY p.pile_id LIMIT ?");
    if(tool=="query_station_comments"&&arguments.value("summary_only").toBool()&&!arguments.value("mine_only").toBool())return QStringLiteral("SELECT c.station_id,s.station_name,ROUND(AVG(c.rating),2),COUNT(*),MAX(c.created_at) FROM station_comments c JOIN charging_stations s ON s.station_id=c.station_id WHERE (?<=0 OR c.station_id=?) GROUP BY c.station_id,s.station_name ORDER BY AVG(c.rating) DESC,COUNT(*) DESC,c.station_id LIMIT ?");
    if(tool=="query_station_comments")return QStringLiteral("SELECT c.comment_id,c.station_id,s.station_name,COALESCE(NULLIF(TRIM(u.nickname),''),'匿名用户'),c.content,c.rating,c.created_at,(SELECT COUNT(*) FROM comment_likes l WHERE l.comment_id=c.comment_id) FROM station_comments c JOIN charging_stations s ON s.station_id=c.station_id LEFT JOIN users u ON u.user_id=c.user_id WHERE (?<=0 OR c.station_id=?) AND (?=0 OR c.user_id=?) ORDER BY c.created_at DESC,c.comment_id DESC LIMIT ?");
    return {};
}
QJsonObject approvedToolCall(QJsonObject requested,qint64 userId)
{
    const QString tool=requested.value("tool").toString();const QJsonObject arguments=requested.value("arguments").toObject();const QString sql=approvedSelect(tool,arguments);if(sql.isEmpty()){requested.remove("sql");return requested;}requested.insert("sql",sql);QJsonArray bindings;
    if(tool=="query_current_user")bindings.append(userId);
    else if(tool=="query_stations"){const QString keyword=arguments.value("keyword").toString().trimmed().left(60);bindings={keyword,"%"+keyword+"%","%"+keyword+"%",qBound(1,arguments.value("limit").toInt(20),50)};}
    else if(tool=="query_piles"){const qint64 stationId=arguments.value("station_id").toInteger();QString status=arguments.value("status").toString();if(!QStringList{"idle","in_use","reserved","fault"}.contains(status))status.clear();bindings={stationId,stationId,status,status,qBound(1,arguments.value("limit").toInt(50),100)};}
    else if(tool=="query_station_comments"){const qint64 stationId=arguments.value("station_id").toInteger();const bool mineOnly=arguments.value("mine_only").toBool();const int limit=qBound(1,arguments.value("limit").toInt(20),50);if(arguments.value("summary_only").toBool()&&!mineOnly)bindings={stationId,stationId,limit};else bindings={stationId,stationId,mineOnly?1:0,userId,limit};}
    requested.insert("bindings",bindings);return requested;
}
QJsonObject executeReadOnlyTool(QSqlDatabase &database,qint64 userId,const QJsonObject &requested)
{
    const QString tool=requested.value("tool").toString();const QJsonObject arguments=requested.value("arguments").toObject();QJsonObject context{{"tool_call",requested}};QJsonArray rows;
    if(tool=="none" || tool.isEmpty()){context.insert("rows",rows);return context;}
    const QString sql=requested.value("sql").toString();if(sql.isEmpty()||sql!=approvedSelect(tool,arguments)||!sql.trimmed().startsWith("SELECT",Qt::CaseInsensitive)||sql.contains(';')){context.insert("tool_error",QStringLiteral("SELECT语句未通过白名单校验"));context.insert("rows",rows);return context;}
    if(tool=="query_current_user"){
        QSqlQuery q(database);q.prepare(sql);q.addBindValue(userId);
        if(q.exec()&&q.next())rows.append(QJsonObject{{"user_id",q.value(0).toLongLong()},{"phone",q.value(1).toString()},{"nickname",q.value(2).toString()},{"balance",q.value(3).toDouble()},{"register_time",q.value(4).toString()},{"status",q.value(5).toString()}});
    }else if(tool=="query_stations"){
        const QString keyword=arguments.value("keyword").toString().trimmed().left(60);const int limit=qBound(1,arguments.value("limit").toInt(20),50);const QString like="%"+keyword+"%";
        QSqlQuery q(database);q.prepare(sql);q.addBindValue(keyword);q.addBindValue(like);q.addBindValue(like);q.addBindValue(limit);
        if(q.exec())while(q.next())rows.append(QJsonObject{{"station_id",q.value(0).toLongLong()},{"station_name",q.value(1).toString()},{"address",q.value(2).toString()},{"longitude",q.value(3).toDouble()},{"latitude",q.value(4).toDouble()},{"price_per_kwh",q.value(5).toDouble()}});
    }else if(tool=="query_piles"){
        const qint64 stationId=arguments.value("station_id").toInteger();QString status=arguments.value("status").toString();if(!QStringList{"idle","in_use","reserved","fault"}.contains(status))status.clear();const int limit=qBound(1,arguments.value("limit").toInt(50),100);
        QSqlQuery q(database);q.prepare(sql);q.addBindValue(stationId);q.addBindValue(stationId);q.addBindValue(status);q.addBindValue(status);q.addBindValue(limit);
        if(q.exec())while(q.next())rows.append(QJsonObject{{"pile_id",q.value(0).toLongLong()},{"station_id",q.value(1).toLongLong()},{"station_name",q.value(2).toString()},{"pile_number",q.value(3).toString()},{"pile_type",q.value(4).toString()},{"power_kw",q.value(5).toDouble()},{"status",q.value(6).toString()},{"total_charge_count",q.value(7).toInt()},{"total_charge_duration",q.value(8).toDouble()}});
    }else if(tool=="query_station_comments"){
        const qint64 stationId=arguments.value("station_id").toInteger();const bool mineOnly=arguments.value("mine_only").toBool();const bool summaryOnly=arguments.value("summary_only").toBool();const int limit=qBound(1,arguments.value("limit").toInt(20),50);
        QSqlQuery q(database);
        if(summaryOnly&&!mineOnly){q.prepare(sql);q.addBindValue(stationId);q.addBindValue(stationId);q.addBindValue(limit);if(q.exec())while(q.next())rows.append(QJsonObject{{"station_id",q.value(0).toLongLong()},{"station_name",q.value(1).toString()},{"average_rating",q.value(2).toDouble()},{"comment_count",q.value(3).toInt()},{"latest_comment_at",q.value(4).toString()}});}
        else{q.prepare(sql);q.addBindValue(stationId);q.addBindValue(stationId);q.addBindValue(mineOnly?1:0);q.addBindValue(userId);q.addBindValue(limit);if(q.exec())while(q.next())rows.append(QJsonObject{{"comment_id",q.value(0).toLongLong()},{"station_id",q.value(1).toLongLong()},{"station_name",q.value(2).toString()},{"display_name",q.value(3).toString()},{"content",q.value(4).toString()},{"rating",q.value(5).toInt()},{"created_at",q.value(6).toString()},{"like_count",q.value(7).toInt()}});}
    }else{context.insert("tool_error",QStringLiteral("工具不在允许列表中"));}
    context.insert("rows",rows);
    return context;
}
}
Result<qint64> ServerApplication::authenticateFeatureUser(QTcpSocket *socket,const Frame &frame) {
    if(frame.payload.value("protocol_version").toInteger()!=ProtocolVersion)
        return Result<qint64>::fail(ErrorCode::ProtocolError,QStringLiteral("协议版本不兼容"));
    const QString session=frame.payload.value("data").toObject().value("session_id").toString();
    if(adminSessions_.contains(session) || !connectionSessions_.value(socket).contains(session) || !sessionManager_.validateAndTouch(session))
        return Result<qint64>::fail(ErrorCode::Unauthorized,QStringLiteral("请使用有效用户账号登录"));
    const qint64 user=sessionManager_.authenticatedUserId(session);
    auto valid=UserService().queryUserInfo(mainDatabase_,user);
    if(!valid.success) return Result<qint64>::fail(valid.code,valid.message);
    return Result<qint64>::ok(user);
}
void ServerApplication::sendFeatureResponse(QTcpSocket *socket,const QString &id,quint32 type,const Result<QJsonObject> &r) {
    socket->write(FrameCodec::encode(type,{{"protocol_version",qint64(ProtocolVersion)},{"request_id",id},{"success",r.success},{"code",errorCodeName(r.code)},{"message",r.message},{"data",QJsonObject{{"result",r.data}}}}));
}
void ServerApplication::processMembershipRequest(QTcpSocket *socket,const Frame &frame) {
    const QString id=frame.payload.value("request_id").toString();
    const auto respond=[&](const Result<QJsonObject> &r){sendFeatureResponse(socket,id,quint32(MessageType::MembershipResponse),r);};
    const auto auth=authenticateFeatureUser(socket,frame);
    if(!auth.success){respond(Result<QJsonObject>::fail(auth.code,auth.message));return;}
    const auto data=frame.payload.value("data").toObject(), params=data.value("params").toObject();
    const QString type=data.value("type").toString();
    const MembershipService service;
    if(type=="plans") respond(service.plans(mainDatabase_));
    else if(type=="status") respond(service.status(mainDatabase_,auth.data));
    else if(type=="purchase") respond(service.purchase(mainDatabase_,auth.data,params));
    else if(type=="set_renewal") respond(service.setRenewal(mainDatabase_,auth.data,params));
    else respond(Result<QJsonObject>::fail(ErrorCode::InvalidInput,QStringLiteral("未知会员操作")));
}
void ServerApplication::processConsultRequest(QTcpSocket *socket,const Frame &frame) {
    using R=Result<QJsonObject>;
    const QString id=frame.payload.value("request_id").toString();
    auto respond=[this,guard=QPointer<QTcpSocket>(socket),id](const R &r){if(guard)sendFeatureResponse(guard,id,quint32(MessageType::ConsultResponse),r);};
    const auto auth=authenticateFeatureUser(socket,frame);
    if(!auth.success){respond(R::fail(auth.code,auth.message));return;}
    const auto data=frame.payload.value("data").toObject();
    const QString session=data.value("session_id").toString();
    if(data.value("type").toString()=="clear") {consultHistory_.remove(session);++consultEpoch_[session];respond(R::ok({}));return;}
    const auto entitlement=MembershipService().snapshot(mainDatabase_,auth.data);
    if(!entitlement.success){respond(entitlement);return;}
    if(entitlement.data.value("level").toString()!="SVIP") {respond(R::fail(ErrorCode::Forbidden,QStringLiteral("AI咨询仅向有效SVIP开放，请开通或续费SVIP")));return;}
    const QString question=data.value("params").toObject().value("question").toString().trimmed();
    if(data.value("type").toString()!="ask" || question.isEmpty() || question.size()>2000){respond(R::fail(ErrorCode::InvalidInput,QStringLiteral("请输入1至2000字的充电租赁问题")));return;}
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(consultBusy_.contains(auth.data) || consultBusy_.size()>=4 || now-consultLastAt_.value(session,0)<3000){respond(R::fail(ErrorCode::Busy,QStringLiteral("咨询请求较频繁，请稍后重试")));return;}
    auto knowledge=KnowledgeService().retrieve(mainDatabase_,question);
    if(!knowledge.success){respond(knowledge);return;}
    const auto sources=knowledge.data.value("sources").toArray();
    consultBusy_.insert(auth.data); consultLastAt_.insert(session,now);
    const auto history=consultHistory_.value(session);
    const auto epoch=consultEpoch_.value(session);
    QJsonObject aiSettings;
    QSqlQuery settingsQuery(mainDatabase_);
    if(settingsQuery.exec("SELECT api_key,base_url,model,system_prompt FROM ai_settings WHERE settings_id=1") && settingsQuery.next()){
        aiSettings={{"api_key",settingsQuery.value(0).toString()},{"base_url",settingsQuery.value(1).toString()},{"model",settingsQuery.value(2).toString()},{"system_prompt",settingsQuery.value(3).toString()}};
    }
    auto answer=[this,guard=QPointer<QTcpSocket>(socket),frame,auth,session,question,sources,history,aiSettings,respond,epoch](const QJsonObject &toolCall) mutable {
    const QString tool=toolCall.value("tool").toString();
    const QHash<QString,QString> toolNames{{"query_current_user",QStringLiteral("个人资料")},{"query_stations",QStringLiteral("充电站信息")},{"query_piles",QStringLiteral("充电桩信息")},{"query_station_comments",QStringLiteral("用户评价")}};
    if(toolNames.contains(tool))respond(R::ok({{"partial",true},{"event","tool"},{"phase","started"},{"tool",tool},{"tool_name",toolNames.value(tool)}}));
    const QJsonObject databaseContext=executeReadOnlyTool(mainDatabase_,auth.data,toolCall);
    if(toolNames.contains(tool))respond(R::ok({{"partial",true},{"event","tool"},{"phase","completed"},{"tool",tool},{"tool_name",toolNames.value(tool)},{"row_count",databaseContext.value("rows").toArray().size()},{"tool_call",toolCall},{"tool_result",databaseContext}}));
    consultApiAdapter_->ask(question,sources,history,aiSettings,databaseContext,
        [this,guard,session,respond,epoch](const QString &delta){if(guard&&connectionSessions_.value(guard).contains(session)&&consultEpoch_.value(session)==epoch)respond(R::ok({{"partial",true},{"delta",delta}}));},
        [this,guard,frame,auth,session,question,sources,respond,epoch](R r) mutable {
        consultBusy_.remove(auth.data);
        if(!guard || !connectionSessions_.value(guard).contains(session)) return;
        if(consultEpoch_.value(session)!=epoch){respond(R::fail(ErrorCode::StateConflict,QStringLiteral("会话已清空，请重新提问")));return;}
        const auto currentAuth=authenticateFeatureUser(guard,frame);
        if(!currentAuth.success){respond(R::fail(currentAuth.code,currentAuth.message));return;}
        const auto current=MembershipService().snapshot(mainDatabase_,auth.data);
        if(!current.success){respond(current);return;}
        if(current.data.value("level").toString()!="SVIP"){respond(R::fail(ErrorCode::Forbidden,QStringLiteral("SVIP已到期，请续费后重新提问")));return;}
        if(r.success){
            auto latest=KnowledgeService().retrieve(mainDatabase_,question);
            if(!latest.success){respond(latest);return;}
            for(const auto &old:sources){
                bool found=false;
                for(const auto &v:latest.data.value("sources").toArray())
                    if(v.toObject().value("article_id")==old.toObject().value("article_id") && v.toObject().value("version")==old.toObject().value("version")) found=true;
                if(!found){respond(R::fail(ErrorCode::StateConflict,QStringLiteral("参考知识已更新或停用，请重新提问")));return;}
            }
            QJsonArray references;
            for(const auto &v:sources){auto s=v.toObject();s.remove("content");references.append(s);}
            r.data.insert("sources",references);r.data.insert("generated",true);
            auto &h=consultHistory_[session];
            h.append(QJsonObject{{"role","user"},{"content",question}});
            h.append(QJsonObject{{"role","assistant"},{"content",r.data.value("answer").toString().left(4000)}});
            while(h.size()>6)h.removeFirst();
        }
        respond(r);
    });
    };
    consultApiAdapter_->planTool(question,history,aiSettings,[this,auth,question,respond,answer=std::move(answer)](R plan) mutable {
        if(!plan.success){consultBusy_.remove(auth.data);respond(plan);return;}
        QJsonObject selected=plan.data;const QString planned=selected.value("tool").toString();
        const bool asksRatings=question.contains(QStringLiteral("评价"))||question.contains(QStringLiteral("评分"))||question.contains(QStringLiteral("口碑"))||question.contains(QStringLiteral("热评"));
        if(asksRatings&&(planned=="query_stations"||planned=="none")){selected={{"tool","query_station_comments"},{"arguments",QJsonObject{{"summary_only",true},{"limit",50}}}};}
        else if(planned=="query_station_comments"&&(question.contains(QStringLiteral("最好"))||question.contains(QStringLiteral("最高")))){auto arguments=selected.value("arguments").toObject();if(arguments.value("station_id").toInteger()<=0)arguments.insert("summary_only",true);selected.insert("arguments",arguments);}
        answer(approvedToolCall(selected,auth.data));
    });
}
}
