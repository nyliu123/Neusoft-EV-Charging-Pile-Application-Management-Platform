#include "membership_dialog.h"
#include "user_api_client.h"
#include "user_session_state.h"
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMap>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QToolButton>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <QUuid>
#include <algorithm>

namespace {
QString money(qint64 cents){return QStringLiteral("¥%1").arg(cents/100.0,0,'f',2);}
QString date(qint64 seconds){return seconds>0?QDateTime::fromSecsSinceEpoch(seconds,Qt::OffsetFromUTC,28800).toString("yyyy-MM-dd HH:mm") : QStringLiteral("—");}
QString planName(const QJsonObject &p){return QString("%1 · %2个月").arg(p.value("level").toString()).arg(p.value("months").toInt());}
QLabel *label(const QString &text,const char *style,QWidget *parent){auto *l=new QLabel(text,parent);l->setTextFormat(Qt::PlainText);l->setWordWrap(true);l->setProperty("uiClass",style);return l;}
QLabel *chatAvatar(bool user,QWidget *parent){auto *avatar=new QLabel(parent);avatar->setFixedSize(38,38);avatar->setAlignment(Qt::AlignCenter);avatar->setProperty("uiClass",user?"userChatAvatar":"assistantChatAvatar");if(user){const QPixmap profile(UserSessionState::instance().avatarPath());if(profile.isNull())avatar->setText(QStringLiteral("我"));else avatar->setPixmap(profile.scaled(36,36,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));}else avatar->setPixmap(QPixmap(QStringLiteral(":/images/xiaoqing.png")).scaled(34,34,Qt::KeepAspectRatio,Qt::SmoothTransformation));return avatar;}
QIcon svgIcon(const QString &resource){QSvgRenderer renderer(resource);QPixmap pixmap(48,48);pixmap.setDevicePixelRatio(2.0);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);renderer.render(&painter,QRectF(0,0,24,24));return QIcon(pixmap);}
void styleChatAction(QPushButton *button){
    button->setFlat(true);button->setAutoFillBackground(false);button->setAttribute(Qt::WA_TranslucentBackground);
    button->setStyleSheet(QStringLiteral("QPushButton{background:transparent;border:none;padding:2px 3px;color:#687286;font-size:12px;font-weight:500;}QPushButton:hover,QPushButton:pressed{background:transparent;border:none;color:#4659bf;}"));
}
QString toolFieldTitle(const QString &field){static const QHash<QString,QString> names{{"user_id",QStringLiteral("用户编号")},{"phone",QStringLiteral("手机号")},{"nickname",QStringLiteral("昵称")},{"balance",QStringLiteral("余额")},{"register_time",QStringLiteral("注册时间")},{"status",QStringLiteral("状态")},{"station_id",QStringLiteral("站点编号")},{"station_name",QStringLiteral("充电站")},{"address",QStringLiteral("地址")},{"longitude",QStringLiteral("经度")},{"latitude",QStringLiteral("纬度")},{"price_per_kwh",QStringLiteral("电价")},{"pile_id",QStringLiteral("充电桩编号")},{"pile_number",QStringLiteral("桩号")},{"pile_type",QStringLiteral("类型")},{"power_kw",QStringLiteral("功率(kW)")},{"total_charge_count",QStringLiteral("充电次数")},{"total_charge_duration",QStringLiteral("累计时长")},{"comment_id",QStringLiteral("评价编号")},{"display_name",QStringLiteral("用户")},{"content",QStringLiteral("评价内容")},{"rating",QStringLiteral("评分")},{"average_rating",QStringLiteral("平均评分")},{"comment_count",QStringLiteral("评价数")},{"like_count",QStringLiteral("点赞数")},{"created_at",QStringLiteral("评价时间")},{"latest_comment_at",QStringLiteral("最新评价时间")}};return names.value(field,field);}
QString toolDisplayName(const QString &tool){static const QHash<QString,QString> names{{"query_current_user",QStringLiteral("个人资料查询")},{"query_stations",QStringLiteral("充电站查询")},{"query_piles",QStringLiteral("充电桩查询")},{"query_station_comments",QStringLiteral("用户评价查询")}};return names.value(tool,tool);}
QString toolValueText(const QJsonValue &value){if(value.isBool())return value.toBool()?QStringLiteral("是"):QStringLiteral("否");if(value.isDouble())return QString::number(value.toDouble(),'g',12);if(value.isNull()||value.isUndefined())return QStringLiteral("—");return value.toString();}
class OverlayGeometryFilter final : public QObject {
public:
    OverlayGeometryFilter(QWidget *overlay,QObject *parent):QObject(parent),overlay_(overlay){}
protected:
    bool eventFilter(QObject *watched,QEvent *event) override {
        if(event->type()==QEvent::Resize || event->type()==QEvent::Show){
            if(auto *host=qobject_cast<QWidget *>(watched))
                overlay_->setGeometry(0,8,248,qMax(0,host->height()-8));
        }
        return false;
    }
private:
    QWidget *overlay_;
};
}
MembershipDialog::MembershipDialog(ev::UserApiClient *api,bool openConsult,QWidget *parent):QDialog(parent),api_(api),consultOnly_(openConsult){
    setObjectName("membershipDialog");setWindowTitle(openConsult?QStringLiteral("小轻 AI助手"):QStringLiteral("轻充 · 会员中心"));resize(410,720);setMinimumSize(0,0);
    setAttribute(Qt::WA_DeleteOnClose);setWindowModality(Qt::WindowModal);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(10,10,10,10);root->setSpacing(8);
    auto *top=new QHBoxLayout;
    auto *back=new QPushButton(QStringLiteral("< 返回"),this);back->setProperty("uiClass","text");top->addWidget(back);
    top->addWidget(label(openConsult?QStringLiteral("小轻 AI助手"):QStringLiteral("会员中心"),"pageTitle",this));top->addStretch();
    auto *refresh=new QPushButton(QStringLiteral("刷新权益"),this);refresh->setProperty("uiClass","secondary");top->addWidget(refresh);root->addLayout(top);
    status_=label(QStringLiteral("正在查询会员权益…"),"membershipStatus",this);status_->setObjectName("membershipStatus");root->addWidget(status_);
    notice_=label(QStringLiteral("教学模拟：从模拟钱包扣费，不发生真实支付。"),"muted",this);root->addWidget(notice_);
    refresh->setVisible(!openConsult);status_->setVisible(!openConsult);notice_->setVisible(!openConsult);
    tabs_=new QTabWidget(this);root->addWidget(tabs_,1);
    auto *membership=new QWidget(tabs_);auto *layout=new QVBoxLayout(membership);layout->setContentsMargins(0,12,0,0);
    auto *filter=new QHBoxLayout;billing_=new QComboBox(membership);billing_->setObjectName("membershipBilling");
    filter->addWidget(label(QStringLiteral("自动续费"),"formLabel",membership));billing_->addItem(QStringLiteral("否"),0);billing_->addItem(QStringLiteral("是"),1);filter->addWidget(billing_);filter->addStretch();
    layout->addLayout(filter);
    planLevelTabs_=new QTabWidget(membership);
    const auto addPlanPage=[this](const QString &name,QWidget *&container,QGridLayout *&grid){auto *scroll=new QScrollArea(planLevelTabs_);scroll->setWidgetResizable(true);container=new QWidget(scroll);grid=new QGridLayout(container);grid->setSpacing(12);grid->setContentsMargins(0,8,8,0);grid->setAlignment(Qt::AlignTop);scroll->setWidget(container);planLevelTabs_->addTab(scroll,name);};
    addPlanPage(QStringLiteral("VIP"),vipCardContainer_,vipCards_);addPlanPage(QStringLiteral("SVIP"),svipCardContainer_,svipCards_);layout->addWidget(planLevelTabs_,1);
    if(!openConsult)tabs_->addTab(membership,QString());else membership->hide();
    auto *consult=new QWidget(tabs_);auto *consultRoot=new QGridLayout(consult);consultRoot->setContentsMargins(0,8,0,0);consultRoot->setSpacing(0);
    auto *historyRail=new QWidget(consult);auto *railLayout=new QHBoxLayout(historyRail);railLayout->setContentsMargins(0,0,18,0);
    auto *historyPanel=new QFrame(historyRail);historyPanel->setObjectName("chatHistoryPanel");auto *historyLayout=new QVBoxLayout(historyPanel);historyLayout->setContentsMargins(12,14,12,12);historyLayout->setSpacing(10);railLayout->addWidget(historyPanel);
    auto *historyHeader=new QHBoxLayout;auto *historyTitle=label(QStringLiteral("历史会话"),"sectionTitle",historyPanel);historyHeader->addWidget(historyTitle);historyHeader->addStretch();historyLayout->addLayout(historyHeader);
    auto *collapseHistory=new QToolButton(historyRail);collapseHistory->setIcon(svgIcon(QStringLiteral(":/icons/chat-back.svg")));collapseHistory->setIconSize(QSize(22,22));collapseHistory->setObjectName("collapseHistory");collapseHistory->setStyleSheet(QStringLiteral("QToolButton { background:#f3f4f8; border:none; border-radius:10px; } QToolButton:hover { background:#f3f4f8; }"));collapseHistory->setFixedSize(36,36);collapseHistory->move(212,18);collapseHistory->raise();
    auto *newChat=new QPushButton(QStringLiteral("+ 新对话"),historyPanel);newChat->setObjectName("newChatButton");newChat->setProperty("uiClass","secondary");historyLayout->addWidget(newChat);
    chatSessionList_=new QListWidget(historyPanel);chatSessionList_->setObjectName("chatSessionList");chatSessionList_->setSpacing(4);historyLayout->addWidget(chatSessionList_,1);
    auto *deleteChat=new QPushButton(QStringLiteral("清空历史会话"),historyPanel);deleteChat->setProperty("uiClass","danger");historyLayout->addWidget(deleteChat);
    auto *chatPanel=new QWidget(consult);auto *cl=new QVBoxLayout(chatPanel);cl->setContentsMargins(0,0,0,0);cl->setSpacing(10);consultHint_=label({},"muted",chatPanel);consultHint_->hide();
    auto *collapsedHeader=new QHBoxLayout;auto *openHistory=new QToolButton(chatPanel);openHistory->setObjectName("openChatHistory");openHistory->setAccessibleName(QStringLiteral("打开历史会话"));openHistory->setIcon(svgIcon(QStringLiteral(":/icons/chat-menu.svg")));openHistory->setIconSize(QSize(22,22));openHistory->setFixedSize(36,36);collapsedHeader->addWidget(openHistory);collapsedHeader->addStretch();cl->addLayout(collapsedHeader);
    auto *messagesScroll=new QScrollArea(chatPanel);messagesScroll->setObjectName("chatMessagesScroll");messagesScroll->setWidgetResizable(true);messagesScroll->setFrameShape(QFrame::NoFrame);
    chatMessagesContainer_=new QWidget(messagesScroll);chatMessagesLayout_=new QVBoxLayout(chatMessagesContainer_);chatMessagesLayout_->setContentsMargins(18,16,18,16);chatMessagesLayout_->setSpacing(14);chatMessagesLayout_->addStretch();messagesScroll->setWidget(chatMessagesContainer_);cl->addWidget(messagesScroll,1);
    auto *composer=new QFrame(chatPanel);composer->setObjectName("chatComposer");auto *composerLayout=new QVBoxLayout(composer);composerLayout->setContentsMargins(10,8,10,8);composerLayout->setSpacing(8);
    question_=new QPlainTextEdit(composer);question_->setObjectName("consultQuestion");question_->setPlaceholderText(QStringLiteral("给小轻发消息…"));question_->setMaximumHeight(96);composerLayout->addWidget(question_,1);
    question_->installEventFilter(this);
    send_=new QPushButton(QStringLiteral("一键咨询"),composer);send_->setObjectName("consultSend");send_->setProperty("uiClass","primary");send_->setMinimumHeight(42);composerLayout->addWidget(send_);cl->addWidget(composer);
    consultRoot->addWidget(chatPanel,0,0);consult->installEventFilter(new OverlayGeometryFilter(historyRail,consult));historyRail->setGeometry(0,8,248,qMax(0,consult->height()-8));historyRail->raise();
    if(openConsult){tabs_->addTab(consult,QStringLiteral("小轻 AI助手"));}else consult->hide();
    tabs_->tabBar()->hide();
    tabs_->setCurrentIndex(0);
    connect(refresh,&QPushButton::clicked,this,[this]{if(!busy_)reload();});
    connect(back,&QPushButton::clicked,this,&QDialog::reject);
    connect(billing_,&QComboBox::currentIndexChanged,this,[this]{renderPlans();});
    connect(send_,&QPushButton::clicked,this,&MembershipDialog::ask);
    connect(newChat,&QPushButton::clicked,this,[this]{createChatSession();api_->consult("clear",{},this,[](bool,const QJsonObject &,const QString &){});});
    connect(deleteChat,&QPushButton::clicked,this,&MembershipDialog::deleteChatSession);
    connect(openHistory,&QToolButton::clicked,this,[historyRail]{historyRail->show();historyRail->raise();});
    connect(collapseHistory,&QToolButton::clicked,this,[historyRail]{historyRail->hide();});
    historyRail->hide();
    connect(chatSessionList_,&QListWidget::currentRowChanged,this,[this](int row){if(row>=0)selectChatSession(chatSessionList_->item(row)->data(Qt::UserRole).toString());});
    connect(api_,&ev::UserApiClient::sessionExpired,this,[this]{busy_=false;QDialog::reject();});
    if(openConsult)loadChatSessions();
    reload();
}
bool MembershipDialog::eventFilter(QObject *watched,QEvent *event){
    if(watched==question_ && event->type()==QEvent::KeyPress){
        auto *key=static_cast<QKeyEvent *>(event);
        if(key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter){
            if(key->modifiers().testFlag(Qt::ShiftModifier))return QDialog::eventFilter(watched,event);
            if(send_->isEnabled() && !question_->toPlainText().trimmed().isEmpty())ask();
            return true;
        }
    }
    return QDialog::eventFilter(watched,event);
}
void MembershipDialog::reject(){if(busy_){notice_->setText(QStringLiteral("正在确认操作结果，请稍候；请勿重复购买。"));return;}QDialog::reject();}
void MembershipDialog::setBusy(bool busy){busy_=busy;if(vipCardContainer_)vipCardContainer_->setEnabled(!busy);if(svipCardContainer_)svipCardContainer_->setEnabled(!busy);if(billing_)billing_->setEnabled(!busy);}
void MembershipDialog::reload(){
    api_->membership("status",{},this,[this](bool ok,const QJsonObject &r,const QString &message){
        if(!ok){status_->setText(message);return;}state_=r;
        const bool valid=r.value("valid").toBool();const QString level=valid?r.value("level").toString():QStringLiteral("普通用户");
        status_->setText(valid?QString("%1 · 充电 %2 折\n当前权益 %3 至 %4 · 已购权益至 %5").arg(level).arg(r.value("discount_bps").toInt()/1000.0,0,'g',3).arg(date(r.value("starts_at").toInteger()),date(r.value("expires_at").toInteger()),date(r.value("paid_until").toInteger())):QStringLiteral("尚无有效会员 · 选择适合你的充电权益"));
        if(!consultOnly_)renderPlans();
        consultHint_->clear();consultHint_->hide();
    });
    if(consultOnly_)return;
    api_->membership("plans",{},this,[this](bool ok,const QJsonObject &r,const QString &message){if(!ok){notice_->setText(message);return;}plans_=r.value("plans").toArray();renderPlans();});
}
void MembershipDialog::renderPlans(){
    const auto clear=[](QGridLayout *grid){while(auto *item=grid->takeAt(0)){delete item->widget();delete item;}};clear(vipCards_);clear(svipCards_);
    int vipIndex=0,svipIndex=0;
    for(const auto &value:plans_){const auto p=value.toObject();if(p.value("recurring").toInt()!=billing_->currentData().toInt())continue;const bool vip=p.value("level").toString()=="VIP";auto *container=vip?vipCardContainer_:svipCardContainer_;auto *grid=vip?vipCards_:svipCards_;int &index=vip?vipIndex:svipIndex;
        auto *card=new QFrame(container);card->setProperty("uiClass","memberPlan");card->setProperty("level",p.value("level").toString());card->setMinimumHeight(155);auto *l=new QVBoxLayout(card);l->setContentsMargins(18,14,18,14);l->setSpacing(7);
        l->addWidget(label(planName(p),"sectionTitle",card));
        l->addWidget(label(money(p.value("price_cent").toInteger()),"stationPrice",card));
        auto *benefit=label(QString("充电 %1 折 · %2").arg(p.value("discount_bps").toInt()/1000.0,0,'g',3).arg(p.value("level").toString()=="SVIP"?QStringLiteral("含AI咨询"):QStringLiteral("不含AI咨询")),"muted",card);benefit->setWordWrap(true);l->addWidget(benefit);
        const bool sameCurrentPlan=state_.value("valid").toBool() && state_.value("plan_id").toInteger()==p.value("plan_id").toInteger();
        auto *b=new QPushButton(sameCurrentPlan?QStringLiteral("续费"):QStringLiteral("开通"),card);b->setObjectName(QString("buyPlan%1").arg(p.value("plan_id").toInt()));b->setProperty("uiClass",p.value("level").toString()=="SVIP"?"primary":"secondary");l->addWidget(b);connect(b,&QPushButton::clicked,this,[this,p]{buy(p);});grid->addWidget(card,index,0);++index;
    }
    for(auto *grid:{vipCards_,svipCards_})grid->setColumnStretch(0,1);
}
void MembershipDialog::buy(const QJsonObject &p){
    if(busy_)return;
    QDialog confirm(this);confirm.setWindowTitle(QStringLiteral("确认模拟会员购买"));auto *l=new QVBoxLayout(&confirm);
    const bool sameCurrentPlan=state_.value("valid").toBool() && state_.value("plan_id").toInteger()==p.value("plan_id").toInteger();
    const QString timing=!state_.value("valid").toBool()?QStringLiteral("付款后立即生效。")
        :sameCurrentPlan?QStringLiteral("续费时长将在当前会员有效期后顺延。")
                        :QStringLiteral("新套餐将在当前会员失效后生效。");
    l->addWidget(label(QString("%1\n本次从模拟钱包扣费 %2，充电 %3 折。\n%4\n固定期限套餐不新增自动续费，已有续费授权需另行取消。").arg(planName(p),money(p.value("price_cent").toInteger())).arg(p.value("discount_bps").toInt()/1000.0,0,'g',3).arg(timing),"formLabel",&confirm));
    auto *consent=new QCheckBox(QString("我单独同意每 %1 个月按 %2 从模拟钱包自动续费，可随时取消后续续费。").arg(p.value("months").toInt()).arg(money(p.value("price_cent").toInteger())),&confirm);consent->setObjectName("renewalConsent");consent->setVisible(p.value("recurring").toInt());l->addWidget(consent);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&confirm);buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认扣费"));buttons->button(QDialogButtonBox::Ok)->setEnabled(!p.value("recurring").toInt());
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Ok)->setIcon(QIcon());
    buttons->button(QDialogButtonBox::Cancel)->setIcon(QIcon());
    connect(consent,&QCheckBox::toggled,buttons->button(QDialogButtonBox::Ok),&QPushButton::setEnabled);connect(buttons,&QDialogButtonBox::accepted,&confirm,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&confirm,&QDialog::reject);l->addWidget(buttons);
    if(confirm.exec()!=QDialog::Accepted)return;
    QJsonObject params{{"plan_id",p.value("plan_id")},{"version",p.value("version")},{"renewal_consent",consent->isChecked()},{"operation_key",QUuid::createUuid().toString(QUuid::WithoutBraces)}};
    submitPurchase(params);
}
void MembershipDialog::submitPurchase(const QJsonObject &params){
    setBusy(true);
    api_->membership("purchase",params,this,[this](bool ok,const QJsonObject &r,const QString &message){
        setBusy(false);
        if(ok){UserSessionState::instance().setBalanceCent(r.value("balance_cent").toInteger());notice_->setText(QString("购买已确认 · 支付 %1 · 权益至 %2%3").arg(money(r.value("paid_cent").toInteger()),date(r.value("expires_at").toInteger()),r.value("replayed").toBool()?QStringLiteral("（重复请求未再次扣费）"):QString()));}
        else notice_->setText(message);
        reload();
    });
}
void MembershipDialog::ask(){
    const QString question=question_->toPlainText().trimmed();if(question.isEmpty() || question.size()>2000){consultHint_->setText(QStringLiteral("请输入1至2000字的问题。"));return;}
    if(!state_.value("valid").toBool() || state_.value("level").toString()!="SVIP"){
        QWidget *owner=parentWidget();auto *membership=new MembershipDialog(api_,false,owner);membership->show();accept();return;
    }
    question_->clear();sendQuestion(question,true);
}

int MembershipDialog::chatSessionIndex(const QString &sessionId) const{
    for(int i=0;i<chatSessions_.size();++i)if(chatSessions_.at(i).toObject().value("id").toString()==sessionId)return i;
    return -1;
}
void MembershipDialog::saveChatSessions() const{
    QSettings().setValue(QString("chat/sessions/%1").arg(UserSessionState::instance().userId()),QJsonDocument(chatSessions_).toJson(QJsonDocument::Compact));
}
void MembershipDialog::loadChatSessions(){
    chatSessions_=QJsonDocument::fromJson(QSettings().value(QString("chat/sessions/%1").arg(UserSessionState::instance().userId())).toByteArray()).array();
    if(chatSessions_.isEmpty()){createChatSession();return;}
    currentChatSessionId_=chatSessions_.first().toObject().value("id").toString();renderChatSessions();renderChatMessages();
}
void MembershipDialog::createChatSession(){
    for(const auto &value:chatSessions_){const auto existing=value.toObject();if(existing.value("messages").toArray().isEmpty()){currentChatSessionId_=existing.value("id").toString();editingMessageIndex_=-1;send_->setText(QStringLiteral("一键咨询"));renderChatSessions();renderChatMessages();question_->clear();return;}}
    const QJsonObject session{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"title",QStringLiteral("新对话")},{"messages",QJsonArray{}}};
    chatSessions_.insert(0,session);currentChatSessionId_=session.value("id").toString();editingMessageIndex_=-1;send_->setText(QStringLiteral("一键咨询"));saveChatSessions();renderChatSessions();renderChatMessages();question_->clear();
}
void MembershipDialog::renderChatSessions(){
    chatSessionList_->blockSignals(true);chatSessionList_->clear();int selected=-1;int rowIndex=0;
    for(int i=0;i<chatSessions_.size();++i){const auto session=chatSessions_.at(i).toObject();if(session.value("messages").toArray().isEmpty())continue;const QString sessionId=session.value("id").toString();auto *item=new QListWidgetItem(chatSessionList_);item->setData(Qt::UserRole,sessionId);item->setSizeHint(QSize(190,38));auto *row=new QWidget(chatSessionList_);auto *rowLayout=new QHBoxLayout(row);rowLayout->setContentsMargins(8,2,3,2);rowLayout->setSpacing(3);auto *open=new QPushButton(session.value("title").toString(),row);open->setProperty("uiClass","chatSessionOpen");open->setToolTip(session.value("title").toString());rowLayout->addWidget(open,1);auto *remove=new QToolButton(row);remove->setIcon(QIcon(QStringLiteral(":/icons/trash-red.svg")));remove->setIconSize(QSize(17,17));remove->setToolTip(QStringLiteral("删除会话"));remove->setAccessibleName(QStringLiteral("删除会话"));remove->setProperty("uiClass","chatSessionDelete");remove->setFixedSize(26,26);rowLayout->addWidget(remove);chatSessionList_->setItemWidget(item,row);connect(open,&QPushButton::clicked,this,[this,item,sessionId]{chatSessionList_->setCurrentItem(item);selectChatSession(sessionId);});connect(remove,&QToolButton::clicked,this,[this,sessionId]{deleteChatSessionById(sessionId);});if(sessionId==currentChatSessionId_)selected=rowIndex;++rowIndex;}
    chatSessionList_->setCurrentRow(selected);chatSessionList_->blockSignals(false);
}
void MembershipDialog::selectChatSession(const QString &sessionId){
    if(chatSessionIndex(sessionId)<0)return;
    currentChatSessionId_=sessionId;
    editingMessageIndex_=-1;
    send_->setText(QStringLiteral("一键咨询"));
    question_->clear();
    renderChatMessages();
}
void MembershipDialog::deleteChatSession(){
    chatSessions_=QJsonArray{};currentChatSessionId_.clear();editingMessageIndex_=-1;createChatSession();api_->consult("clear",{},this,[](bool,const QJsonObject &,const QString &){});
}
void MembershipDialog::deleteChatSessionById(const QString &sessionId){
    const int index=chatSessionIndex(sessionId);if(index<0)return;const bool deletingCurrent=sessionId==currentChatSessionId_;chatSessions_.removeAt(index);
    if(deletingCurrent){if(chatSessions_.isEmpty()){currentChatSessionId_.clear();createChatSession();return;}currentChatSessionId_=chatSessions_.at(qMin(index,chatSessions_.size()-1)).toObject().value("id").toString();editingMessageIndex_=-1;question_->clear();}
    saveChatSessions();renderChatSessions();renderChatMessages();
}
void MembershipDialog::copyChatText(const QString &text) const{QApplication::clipboard()->setText(text);}
void MembershipDialog::editUserMessage(int messageIndex){
    const int sessionIndex=chatSessionIndex(currentChatSessionId_);if(sessionIndex<0)return;const auto messages=chatSessions_.at(sessionIndex).toObject().value("messages").toArray();if(messageIndex<0||messageIndex>=messages.size())return;
    question_->setPlainText(messages.at(messageIndex).toObject().value("text").toString());editingMessageIndex_=messageIndex;send_->setText(QStringLiteral("保存并发送"));question_->setFocus();
}
void MembershipDialog::retryMessage(int messageIndex){
    const int sessionIndex=chatSessionIndex(currentChatSessionId_);if(sessionIndex<0)return;auto session=chatSessions_.at(sessionIndex).toObject();auto messages=session.value("messages").toArray();if(messageIndex<0||messageIndex>=messages.size())return;
    const QString retryQuestion=messages.at(messageIndex).toObject().value("question").toString();messages.removeAt(messageIndex);session.insert("messages",messages);chatSessions_.replace(sessionIndex,session);saveChatSessions();renderChatMessages();sendQuestion(retryQuestion,false);
}
void MembershipDialog::renderChatMessages(){
    while(chatMessagesLayout_->count()>1){auto *item=chatMessagesLayout_->takeAt(0);delete item->widget();delete item;}
    const int sessionIndex=chatSessionIndex(currentChatSessionId_);if(sessionIndex<0)return;const auto messages=chatSessions_.at(sessionIndex).toObject().value("messages").toArray();
    QJsonArray visibleMessages=messages;
    if(visibleMessages.isEmpty())visibleMessages.append(QJsonObject{{"role","assistant"},{"text",QStringLiteral("Hello，我是小轻！\n有什么充电、订单或会员问题，都可以告诉我。")},{"state","greeting"}});
    for(int i=0;i<visibleMessages.size();++i){
        const auto message=visibleMessages.at(i).toObject();const bool user=message.value("role").toString()=="user";const bool greeting=message.value("state").toString()=="greeting";
        auto *rowWidget=new QWidget(chatMessagesContainer_);rowWidget->setProperty("uiClass","chatMessageRow");auto *row=new QHBoxLayout(rowWidget);row->setContentsMargins(0,0,0,0);row->setSpacing(9);
        auto *messageGroup=new QWidget(rowWidget);messageGroup->setProperty("uiClass","chatMessageGroup");messageGroup->setMaximumWidth(620);messageGroup->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);auto *messageLayout=new QVBoxLayout(messageGroup);messageLayout->setContentsMargins(0,0,0,0);messageLayout->setSpacing(2);
        auto *bubble=new QFrame(messageGroup);bubble->setProperty("uiClass","chatBubble");bubble->setProperty("role",user?"user":"assistant");bubble->setProperty("state",message.value("state").toString());auto *bubbleLayout=new QVBoxLayout(bubble);bubbleLayout->setContentsMargins(15,10,15,10);bubbleLayout->setSpacing(4);
        if(!user&&!message.value("tool_status").toString().isEmpty()){
            auto *toolCard=new QFrame(bubble);toolCard->setProperty("uiClass","chatToolCard");auto *toolLayout=new QHBoxLayout(toolCard);toolLayout->setContentsMargins(9,4,5,4);toolLayout->setSpacing(8);toolLayout->addWidget(label(message.value("tool_status").toString(),"chatToolStatus",toolCard),1);
            if(message.contains("tool_result")){auto *details=new QPushButton(QStringLiteral("查看详情"),toolCard);details->setProperty("uiClass","chatToolDetails");toolLayout->addWidget(details,0,Qt::AlignRight);const QJsonObject call=message.value("tool_call").toObject(),result=message.value("tool_result").toObject();connect(details,&QPushButton::clicked,this,[this,call,result]{QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("工具调用详情"));dialog.resize(820,560);auto *layout=new QVBoxLayout(&dialog);const QString toolName=toolDisplayName(call.value("tool").toString());const QJsonArray rows=result.value("rows").toArray();layout->addWidget(label(QStringLiteral("查询类型：%1\n共返回 %2 条结果").arg(toolName).arg(rows.size()),"sectionTitle",&dialog));if(rows.isEmpty())layout->addWidget(label(QStringLiteral("没有查询到匹配数据。"),"muted",&dialog),1);else{QStringList fields;for(const QString &key:rows.first().toObject().keys())fields.append(key);auto *table=new QTableWidget(rows.size(),fields.size(),&dialog);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);for(int column=0;column<fields.size();++column)table->setHorizontalHeaderItem(column,new QTableWidgetItem(toolFieldTitle(fields.at(column))));for(int row=0;row<rows.size();++row){const auto object=rows.at(row).toObject();for(int column=0;column<fields.size();++column){auto *cell=new QTableWidgetItem(toolValueText(object.value(fields.at(column))));cell->setToolTip(cell->text());table->setItem(row,column,cell);}}table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setStretchLastSection(true);layout->addWidget(table,1);}layout->addWidget(label(QStringLiteral("执行的 SELECT 语句"),"formLabel",&dialog));auto *sqlView=new QPlainTextEdit(&dialog);sqlView->setReadOnly(true);sqlView->setMaximumHeight(90);sqlView->setPlainText(call.value("sql").toString());layout->addWidget(sqlView);QStringList bindingTexts;for(const auto &binding:call.value("bindings").toArray())bindingTexts.append(toolValueText(binding));layout->addWidget(label(QStringLiteral("绑定参数：%1").arg(bindingTexts.isEmpty()?QStringLiteral("无"):bindingTexts.join(QStringLiteral(" · "))),"muted",&dialog));auto *close=new QPushButton(QStringLiteral("关闭"),&dialog);close->setProperty("uiClass","primary");connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);layout->addWidget(close,0,Qt::AlignRight);dialog.exec();});}
            bubbleLayout->addWidget(toolCard);
        }
        auto *content=label(message.value("text").toString(),"chatMessageText",bubble);
        if(!user)content->setTextFormat(Qt::MarkdownText);
        if(user)content->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        content->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard|Qt::LinksAccessibleByMouse);bubbleLayout->addWidget(content);
        messageLayout->addWidget(bubble);
        {
            auto *actions=new QHBoxLayout;
            actions->setContentsMargins(2,0,2,0);actions->setSpacing(8);actions->setAlignment(user?Qt::AlignRight:Qt::AlignLeft);
            auto *copy=new QPushButton(QIcon(QStringLiteral(":/icons/copy.svg")),QStringLiteral("复制"),messageGroup);
            copy->setObjectName(user?QStringLiteral("copyUserMessage"):QStringLiteral("copyAssistantMessage"));
            copy->setProperty("uiClass","chatAction");copy->setIconSize(QSize(14,14));styleChatAction(copy);
            actions->addWidget(copy);
            connect(copy,&QPushButton::clicked,this,[this,copy,text=message.value("text").toString()]{copyChatText(text);copy->setText(QStringLiteral("已复制"));});
            if(user && !greeting){
                auto *edit=new QPushButton(QIcon(QStringLiteral(":/icons/edit.svg")),QStringLiteral("编辑"),messageGroup);
                edit->setObjectName(QStringLiteral("editUserMessage"));
                edit->setProperty("uiClass","chatAction");edit->setIconSize(QSize(14,14));styleChatAction(edit);
                actions->addWidget(edit);
                connect(edit,&QPushButton::clicked,this,[this,i]{editUserMessage(i);});
            }else if(!greeting && message.value("state").toString()=="failed"){
                auto *retry=new QPushButton(QIcon(QStringLiteral(":/icons/retry.svg")),QStringLiteral("重试"),messageGroup);
                retry->setObjectName(QStringLiteral("retryAssistantMessage"));
                retry->setProperty("uiClass","chatAction");retry->setIconSize(QSize(14,14));styleChatAction(retry);
                actions->addWidget(retry);
                connect(retry,&QPushButton::clicked,this,[this,i]{retryMessage(i);});
            }
            messageLayout->addLayout(actions);
        }
        auto *identity=new QWidget(rowWidget);identity->setProperty("uiClass","chatIdentity");auto *identityLayout=new QVBoxLayout(identity);identityLayout->setContentsMargins(0,0,0,0);identityLayout->setSpacing(3);auto *sender=label(user?QStringLiteral("你"):QStringLiteral("小轻"),"chatSender",identity);sender->setAlignment(Qt::AlignCenter);identityLayout->addWidget(sender);identityLayout->addWidget(chatAvatar(user,identity),0,Qt::AlignCenter);if(user){row->addStretch();row->addWidget(messageGroup,0,Qt::AlignTop);row->addWidget(identity,0,Qt::AlignTop);}else{row->addWidget(identity,0,Qt::AlignTop);row->addWidget(messageGroup,0,Qt::AlignTop);row->addStretch();}chatMessagesLayout_->insertWidget(chatMessagesLayout_->count()-1,rowWidget);
    }
}
void MembershipDialog::sendQuestion(const QString &question,bool appendUser){
    int sessionIndex=chatSessionIndex(currentChatSessionId_);if(sessionIndex<0){createChatSession();sessionIndex=chatSessionIndex(currentChatSessionId_);}auto session=chatSessions_.at(sessionIndex).toObject();auto messages=session.value("messages").toArray();
    if(appendUser){if(editingMessageIndex_>=0&&editingMessageIndex_<messages.size()){while(messages.size()>editingMessageIndex_+1)messages.removeLast();auto edited=messages.at(editingMessageIndex_).toObject();edited.insert("text",question);messages.replace(editingMessageIndex_,edited);editingMessageIndex_=-1;send_->setText(QStringLiteral("一键咨询"));}else messages.append(QJsonObject{{"role","user"},{"text",question},{"state","sent"}});if(session.value("title").toString()==QStringLiteral("新对话"))session.insert("title",question.left(18));}
    const QString token=QUuid::createUuid().toString(QUuid::WithoutBraces);messages.append(QJsonObject{{"role","assistant"},{"text",QStringLiteral("小轻正在思考…")},{"state","loading"},{"question",question},{"token",token}});session.insert("messages",messages);chatSessions_.replace(sessionIndex,session);saveChatSessions();renderChatSessions();renderChatMessages();send_->setEnabled(false);const QString sessionId=currentChatSessionId_;
    api_->consult("ask",{{"question",question}},this,[this,sessionId,token](bool ok,const QJsonObject &result,const QString &message){
        const bool partial=ok&&result.value("partial").toBool();if(!partial)send_->setEnabled(true);
        const int si=chatSessionIndex(sessionId);if(si<0)return;auto session=chatSessions_.at(si).toObject();auto messages=session.value("messages").toArray();
        for(int i=0;i<messages.size();++i){auto item=messages.at(i).toObject();if(item.value("token").toString()!=token)continue;
            if(partial){
                if(result.value("event").toString()=="tool"){const QString name=result.value("tool_name").toString();if(result.value("phase").toString()=="started")item.insert("tool_status",QStringLiteral("正在调用：%1…").arg(name));else{item.insert("tool_status",QStringLiteral("已调用：%1 · 返回 %2 条").arg(name).arg(result.value("row_count").toInt()));item.insert("tool_call",result.value("tool_call"));item.insert("tool_result",result.value("tool_result"));}}
                const QString delta=result.value("delta").toString();if(!delta.isEmpty()){const QString text=item.value("state").toString()=="loading"?delta:item.value("text").toString()+delta;item.insert("text",text);item.insert("state","streaming");}
            }
            else if(ok){QString answer=result.value("answer").toString();int sourceIndex=0;for(const auto &value:result.value("sources").toArray()){const auto source=value.toObject();answer+=QString("\n\n[%1] %2 · %3").arg(++sourceIndex).arg(source.value("title").toString(),source.value("source").toString());}item.insert("text",answer);item.insert("state","sent");}
            else{item.insert("text",QStringLiteral("发送失败：")+(message.isEmpty()?QStringLiteral("请稍后重试"):message));item.insert("state","failed");}
            messages.replace(i,item);break;
        }
        session.insert("messages",messages);chatSessions_.replace(si,session);if(!partial)saveChatSessions();if(currentChatSessionId_==sessionId)renderChatMessages();
    });
}
