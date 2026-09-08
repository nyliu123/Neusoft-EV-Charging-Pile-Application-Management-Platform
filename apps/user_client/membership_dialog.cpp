#include "membership_dialog.h"
#include "user_api_client.h"
#include "user_session_state.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QUuid>
#include <algorithm>

namespace {
QString money(qint64 cents){return QStringLiteral("¥%1").arg(cents/100.0,0,'f',2);}
QString date(qint64 seconds){return seconds>0?QDateTime::fromSecsSinceEpoch(seconds,Qt::OffsetFromUTC,28800).toString("yyyy-MM-dd HH:mm") : QStringLiteral("—");}
QString planName(const QJsonObject &p){return QString("%1 · %2%3个月").arg(p.value("level").toString(),p.value("recurring").toInt()?QStringLiteral("连续 "):QStringLiteral("固定 ")).arg(p.value("months").toInt());}
QLabel *label(const QString &text,const char *style,QWidget *parent){auto *l=new QLabel(text,parent);l->setTextFormat(Qt::PlainText);l->setWordWrap(true);l->setProperty("uiClass",style);return l;}
}
MembershipDialog::MembershipDialog(ev::UserApiClient *api,bool openConsult,QWidget *parent):QDialog(parent),api_(api){
    setObjectName("membershipDialog");setWindowTitle(QStringLiteral("轻充 · 会员与AI咨询"));resize(1000,720);setMinimumSize(880,620);
    setAttribute(Qt::WA_DeleteOnClose);setWindowModality(Qt::WindowModal);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(24,20,24,20);root->setSpacing(12);
    auto *top=new QHBoxLayout;
    top->addWidget(label(QStringLiteral("会员中心"),"pageTitle",this));top->addStretch();
    auto *refresh=new QPushButton(QStringLiteral("刷新权益"),this);refresh->setProperty("uiClass","secondary");top->addWidget(refresh);root->addLayout(top);
    status_=label(QStringLiteral("正在查询会员权益…"),"membershipStatus",this);status_->setObjectName("membershipStatus");root->addWidget(status_);
    notice_=label(QStringLiteral("教学模拟：从模拟钱包扣费，不发生真实支付。"),"muted",this);root->addWidget(notice_);
    tabs_=new QTabWidget(this);root->addWidget(tabs_,1);
    auto *membership=new QWidget(tabs_);auto *layout=new QVBoxLayout(membership);layout->setContentsMargins(0,12,0,0);
    auto *filter=new QHBoxLayout;billing_=new QComboBox(membership);billing_->setObjectName("membershipBilling");
    billing_->addItem(QStringLiteral("固定期限 · 不新增自动续费"),0);billing_->addItem(QStringLiteral("连续订阅 · 需单独同意续费"),1);filter->addWidget(billing_);filter->addStretch();
    retryPurchase_=new QPushButton(QStringLiteral("重试上次未确认购买"),membership);retryPurchase_->setObjectName("membershipRetry");filter->addWidget(retryPurchase_);layout->addLayout(filter);
    auto *scroll=new QScrollArea(membership);scroll->setWidgetResizable(true);cardContainer_=new QWidget(scroll);cards_=new QGridLayout(cardContainer_);cards_->setSpacing(12);cards_->setContentsMargins(0,0,8,0);cards_->setAlignment(Qt::AlignTop);scroll->setWidget(cardContainer_);layout->addWidget(scroll,1);
    auto *renewalCard=new QFrame(membership);renewalCard->setProperty("uiClass","card");auto *renewalLayout=new QVBoxLayout(renewalCard);renewalLayout->setContentsMargins(16,12,16,12);
    renewalInfo_=label({},"muted",renewalCard);renewalInfo_->setObjectName("renewalStatus");renewalLayout->addWidget(renewalInfo_);
    auto *renewalActions=new QHBoxLayout;renewalPlan_=new QComboBox(renewalCard);renewalPlan_->setMinimumWidth(230);renewalActions->addWidget(renewalPlan_,1);
    authorize_=new QPushButton(QStringLiteral("确认 / 开启续费"),renewalCard);authorize_->setObjectName("membershipAuthorize");authorize_->setProperty("uiClass","secondary");renewalActions->addWidget(authorize_);
    cancelRenewal_=new QPushButton(QStringLiteral("取消后续续费"),renewalCard);cancelRenewal_->setObjectName("membershipCancelRenewal");renewalActions->addWidget(cancelRenewal_);renewalLayout->addLayout(renewalActions);layout->addWidget(renewalCard);
    tabs_->addTab(membership,QStringLiteral("VIP / SVIP 套餐"));
    auto *consult=new QWidget(tabs_);auto *cl=new QVBoxLayout(consult);cl->setContentsMargins(0,12,0,0);
    consultHint_=label(QStringLiteral("SVIP专属 · 基于已发布知识回答，答案附参考来源；不会代您扣费或操作设备。"),"muted",consult);cl->addWidget(consultHint_);
    chat_=new QPlainTextEdit(consult);chat_->setObjectName("consultTranscript");chat_->setReadOnly(true);chat_->setPlaceholderText(QStringLiteral("可以问：如何开始充电？会员到期会影响已有订单吗？"));cl->addWidget(chat_,1);
    question_=new QPlainTextEdit(consult);question_->setObjectName("consultQuestion");question_->setPlaceholderText(QStringLiteral("输入充电租赁问题（最多2000字，请勿提供密码、手机号等个人资料）"));question_->setMaximumHeight(92);cl->addWidget(question_);
    auto *actions=new QHBoxLayout;auto *clear=new QPushButton(QStringLiteral("清空会话"),consult);actions->addWidget(clear);actions->addStretch();send_=new QPushButton(QStringLiteral("发送问题"),consult);send_->setObjectName("consultSend");send_->setProperty("uiClass","primary");actions->addWidget(send_);cl->addLayout(actions);
    tabs_->addTab(consult,QStringLiteral("SVIP AI咨询"));tabs_->setCurrentIndex(openConsult?1:0);
    pendingPurchase_=QJsonDocument::fromJson(QSettings().value(pendingSettingKey()).toByteArray()).object();retryPurchase_->setVisible(!pendingPurchase_.isEmpty());
    connect(refresh,&QPushButton::clicked,this,[this]{if(!busy_)reload();});
    connect(billing_,&QComboBox::currentIndexChanged,this,[this]{renderPlans();});
    connect(retryPurchase_,&QPushButton::clicked,this,[this]{if(!busy_ && !pendingPurchase_.isEmpty())submitPurchase(pendingPurchase_);});
    connect(cancelRenewal_,&QPushButton::clicked,this,[this]{
        if(busy_ || QMessageBox::question(this,QStringLiteral("取消续费"),QStringLiteral("取消后不再扣取后续周期费用，已购买权益保留到期。确认取消？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        setBusy(true);api_->membership("set_renewal",{{"enabled",false}},this,[this](bool ok,const QJsonObject &,const QString &message){setBusy(false);notice_->setText(ok?QStringLiteral("后续续费已取消，已购权益保持有效。"):message);reload();});
    });
    connect(authorize_,&QPushButton::clicked,this,&MembershipDialog::authorizeRenewal);
    connect(send_,&QPushButton::clicked,this,&MembershipDialog::ask);
    connect(clear,&QPushButton::clicked,this,[this]{++chatGeneration_;chat_->clear();question_->clear();api_->consult("clear",{},this,[](bool,const QJsonObject &,const QString &){});});
    connect(api_,&ev::UserApiClient::sessionExpired,this,[this]{busy_=false;QDialog::reject();});
    reload();
}
QString MembershipDialog::pendingSettingKey() const {return QString("membership/pending/%1").arg(UserSessionState::instance().userId());}
void MembershipDialog::reject(){if(busy_){notice_->setText(QStringLiteral("正在确认操作结果，请稍候；请勿重复购买。"));return;}QDialog::reject();}
void MembershipDialog::setBusy(bool busy){busy_=busy;cardContainer_->setEnabled(!busy);billing_->setEnabled(!busy);retryPurchase_->setEnabled(!busy);cancelRenewal_->setEnabled(!busy);authorize_->setEnabled(!busy);}
void MembershipDialog::reload(){
    api_->membership("status",{},this,[this](bool ok,const QJsonObject &r,const QString &message){
        if(!ok){status_->setText(message);return;}state_=r;
        const bool valid=r.value("valid").toBool();const QString level=valid?r.value("level").toString():QStringLiteral("普通用户");
        status_->setText(valid?QString("%1 · 充电 %2 折\n当前权益 %3 至 %4 · 已购权益至 %5").arg(level).arg(r.value("discount_bps").toInt()/1000.0,0,'g',3).arg(date(r.value("starts_at").toInteger()),date(r.value("expires_at").toInteger()),date(r.value("paid_until").toInteger())):QStringLiteral("尚无有效会员 · 选择适合你的充电权益"));
        const auto sub=r.value("renewal").toObject();const QString s=sub.value("status").toString();
        const QMap<QString,QString> descriptions{{"active",QStringLiteral("已开启")},{"cancelled",QStringLiteral("已取消")},{"needs_confirmation",QStringLiteral("套餐已变更，等待重新确认")},{"insufficient_balance",QStringLiteral("余额不足，续费已停止")},{"missed_cycle",QStringLiteral("错过完整周期，已停止补扣")},{"account_unavailable",QStringLiteral("账号不可用，续费已停止")}};
        renewalInfo_->setText(QString("自动续费：%1 · 下次/原计划时间：%2 · 已授权价格：%3\n取消续费不取消已付费权益；套餐调价须重新确认。").arg(descriptions.value(s,QStringLiteral("未开启")),date(sub.value("next_due").toInteger()),money(sub.value("price_cent").toInteger())));
        consultHint_->setText(valid && level=="SVIP"?QStringLiteral("SVIP权益有效 · 每次提问由服务端复核，答案附已发布知识来源。"):QStringLiteral("当前无有效SVIP；请先开通或续费SVIP，服务端将阻止未授权提问。"));
    });
    api_->membership("plans",{},this,[this](bool ok,const QJsonObject &r,const QString &message){if(!ok){notice_->setText(message);return;}plans_=r.value("plans").toArray();renderPlans();renewalPlan_->clear();for(const auto &v:plans_){auto p=v.toObject();if(p.value("recurring").toInt())renewalPlan_->addItem(planName(p)+" · "+money(p.value("price_cent").toInteger()),p);}});
}
void MembershipDialog::renderPlans(){
    while(auto *item=cards_->takeAt(0)){delete item->widget();delete item;}
    int index=0;
    QList<QJsonObject> visible;
    for(const auto &v:plans_){const auto p=v.toObject();if(p.value("recurring").toInt()==billing_->currentData().toInt())visible.append(p);}
    std::sort(visible.begin(),visible.end(),[](const auto &a,const auto &b){
        if(a.value("months")!=b.value("months"))return a.value("months").toInt()<b.value("months").toInt();
        return a.value("level").toString()>b.value("level").toString();
    });
    for(const auto &p:visible){
        auto *card=new QFrame(cardContainer_);card->setProperty("uiClass","memberPlan");card->setProperty("level",p.value("level").toString());card->setMinimumHeight(155);auto *l=new QVBoxLayout(card);l->setContentsMargins(18,14,18,14);l->setSpacing(7);
        l->addWidget(label(planName(p),"sectionTitle",card));
        l->addWidget(label(money(p.value("price_cent").toInteger()),"stationPrice",card));
        l->addWidget(label(QString("充电 %1 折 · %2").arg(p.value("discount_bps").toInt()/1000.0,0,'g',3).arg(p.value("level").toString()=="SVIP"?QStringLiteral("含AI咨询"):QStringLiteral("不含AI咨询")),"muted",card));
        auto *b=new QPushButton(QStringLiteral("开通 / 续购"),card);b->setObjectName(QString("buyPlan%1").arg(p.value("plan_id").toInt()));b->setProperty("uiClass",p.value("level").toString()=="SVIP"?"primary":"secondary");l->addWidget(b);connect(b,&QPushButton::clicked,this,[this,p]{buy(p);});cards_->addWidget(card,index/2,index%2);++index;
    }
    cards_->setColumnStretch(0,1);cards_->setColumnStretch(1,1);
}
void MembershipDialog::buy(const QJsonObject &p){
    if(busy_)return;
    if(!pendingPurchase_.isEmpty()){notice_->setText(QStringLiteral("有一笔购买结果尚未确认，请先点击“重试上次未确认购买”。"));return;}
    QDialog confirm(this);confirm.setWindowTitle(QStringLiteral("确认模拟会员购买"));auto *l=new QVBoxLayout(&confirm);
    l->addWidget(label(QString("%1\n本次从模拟钱包扣费 %2，充电 %3 折。\n同等级未到期续购将顺延，未到期不支持跨等级购买。\n固定期限套餐不新增自动续费，已有续费授权需另行取消。").arg(planName(p),money(p.value("price_cent").toInteger())).arg(p.value("discount_bps").toInt()/1000.0,0,'g',3),"formLabel",&confirm));
    auto *consent=new QCheckBox(QString("我单独同意每 %1 个月按 %2 从模拟钱包自动续费，可随时取消后续续费。").arg(p.value("months").toInt()).arg(money(p.value("price_cent").toInteger())),&confirm);consent->setObjectName("renewalConsent");consent->setVisible(p.value("recurring").toInt());l->addWidget(consent);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&confirm);buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认扣费"));buttons->button(QDialogButtonBox::Ok)->setEnabled(!p.value("recurring").toInt());
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Ok)->setIcon(QIcon());
    buttons->button(QDialogButtonBox::Cancel)->setIcon(QIcon());
    connect(consent,&QCheckBox::toggled,buttons->button(QDialogButtonBox::Ok),&QPushButton::setEnabled);connect(buttons,&QDialogButtonBox::accepted,&confirm,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&confirm,&QDialog::reject);l->addWidget(buttons);
    if(confirm.exec()!=QDialog::Accepted)return;
    QJsonObject params{{"plan_id",p.value("plan_id")},{"version",p.value("version")},{"renewal_consent",consent->isChecked()},{"operation_key",QUuid::createUuid().toString(QUuid::WithoutBraces)}};
    pendingPurchase_=params;QSettings settings;settings.setValue(pendingSettingKey(),QJsonDocument(params).toJson(QJsonDocument::Compact));settings.sync();
    if(settings.status()!=QSettings::NoError){pendingPurchase_={};notice_->setText(QStringLiteral("无法保存防重复购买记录，本次未发送扣费请求。"));return;}
    submitPurchase(params);
}
void MembershipDialog::submitPurchase(const QJsonObject &params){
    setBusy(true);retryPurchase_->show();
    api_->membership("purchase",params,this,[this](bool ok,const QJsonObject &r,const QString &message){
        setBusy(false);
        const bool uncertain=!ok && (message.contains(QStringLiteral("超时")) || message.contains(QStringLiteral("网络")) || message.contains(QStringLiteral("发送")));
        if(!uncertain){pendingPurchase_={};QSettings().remove(pendingSettingKey());retryPurchase_->hide();}
        if(ok){UserSessionState::instance().setBalanceCent(r.value("balance_cent").toInteger());notice_->setText(QString("购买已确认 · 支付 %1 · 权益至 %2%3").arg(money(r.value("paid_cent").toInteger()),date(r.value("expires_at").toInteger()),r.value("replayed").toBool()?QStringLiteral("（重复请求未再次扣费）"):QString()));}
        else notice_->setText(message);
        reload();
    });
}
void MembershipDialog::authorizeRenewal(){
    if(busy_)return;
    const auto p=renewalPlan_->currentData().toJsonObject();
    if(p.isEmpty())return;
    if(QMessageBox::question(this,QStringLiteral("单独确认续费授权"),QString("同意从当前已购权益到期时起，每 %1 个月从模拟钱包扣除 %2，充电 %3 折？\n本次仅授权，不立即扣费。价格或权益变更后需要重新确认。").arg(p.value("months").toInt()).arg(money(p.value("price_cent").toInteger())).arg(p.value("discount_bps").toInt()/1000.0,0,'g',3),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    setBusy(true);api_->membership("set_renewal",{{"enabled",true},{"plan_id",p.value("plan_id")},{"version",p.value("version")},{"renewal_consent",true}},this,[this](bool ok,const QJsonObject &,const QString &message){setBusy(false);notice_->setText(ok?QStringLiteral("已确认续费授权。"):message);reload();});
}
void MembershipDialog::ask(){
    const QString question=question_->toPlainText().trimmed();if(question.isEmpty() || question.size()>2000){consultHint_->setText(QStringLiteral("请输入1至2000字的问题。"));return;}
    send_->setEnabled(false);question_->clear();chat_->appendPlainText(QStringLiteral("你：")+question);const int generation=chatGeneration_;
    api_->consult("ask",{{"question",question}},this,[this,generation](bool ok,const QJsonObject &r,const QString &message){
        send_->setEnabled(true);if(generation!=chatGeneration_)return;
        if(!ok){chat_->appendPlainText(QStringLiteral("系统提示：")+message+"\n");return;}
        chat_->appendPlainText((r.value("generated").toBool()?QStringLiteral("轻充AI："):QStringLiteral("知识检索提示："))+r.value("answer").toString());
        int index=0;for(const auto &v:r.value("sources").toArray()){const auto s=v.toObject();chat_->appendPlainText(QString("[%1] %2 · 版本%3 · %4").arg(++index).arg(s.value("title").toString()).arg(s.value("version").toInt()).arg(s.value("source").toString()));}chat_->appendPlainText("");
    });
}
