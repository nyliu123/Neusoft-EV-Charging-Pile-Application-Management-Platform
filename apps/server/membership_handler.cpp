#include "server_application.h"
#include "adapters/consult_api_adapter.h"
#include "common/protocol.h"
#include "services/membership_service.h"
#include "services/knowledge_service.h"
#include "services/user_service.h"
#include <QPointer>
#include <QTcpSocket>

namespace ev {
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
    if(sources.isEmpty()){respond(R::ok({{"answer",QStringLiteral("当前已发布知识中没有找到这个问题的依据。请询问充电操作、费用、会员或订单问题，或联系管理员补充知识。")},{"sources",QJsonArray{}},{"generated",false}}));return;}
    consultBusy_.insert(auth.data); consultLastAt_.insert(session,now);
    const auto history=consultHistory_.value(session);
    const auto epoch=consultEpoch_.value(session);
    consultApiAdapter_->ask(question,sources,history,[this,guard=QPointer<QTcpSocket>(socket),frame,auth,session,question,sources,respond,epoch](R r) mutable {
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
}
}
