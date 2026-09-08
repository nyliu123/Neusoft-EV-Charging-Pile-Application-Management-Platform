#include "admin_membership_page.h"
#include "admin_api_client.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace ev {
namespace {
QLabel *heading(const QString &text,QWidget *parent,const char *style="pageTitle"){
    auto *label=new QLabel(text,parent);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setProperty("uiClass",style);return label;
}
}
AdminMembershipPage::AdminMembershipPage(AdminApiClient *api,QWidget *parent):QWidget(parent),api_(api){
    auto *l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(14);
    auto *top=new QHBoxLayout;top->addWidget(heading(QStringLiteral("会员套餐"),this));top->addStretch();auto *refresh=new QPushButton(QStringLiteral("刷新"),this);refresh->setProperty("uiClass","secondary");top->addWidget(refresh);l->addLayout(top);
    notice_=heading(QStringLiteral("12种固定组合 · 金额为教学模拟价格。修改生成新版本，不影响已购权益与已有订单；续费授权需重新确认。"),this,"muted");l->addWidget(notice_);
    table_=new QTableWidget(0,6,this);table_->setObjectName("adminMembershipPlans");table_->setHorizontalHeaderLabels({QStringLiteral("等级 / 版本"),QStringLiteral("期限与续费方式"),QStringLiteral("价格（元）"),QStringLiteral("应付折扣"),QStringLiteral("上架"),QStringLiteral("操作")});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->verticalHeader()->hide();table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setSelectionMode(QAbstractItemView::NoSelection);l->addWidget(table_,1);
    connect(refresh,&QPushButton::clicked,this,&AdminMembershipPage::reload);
}
void AdminMembershipPage::reload(){
    if(busy_)return;
    busy_=true;
    api_->sendQuery("membership_plans",{},this,[this](bool ok,const QJsonObject &r,const QString &message){
        busy_=false;if(!ok){notice_->setText(message);return;}
        const auto plans=r.value("plans").toArray();table_->setRowCount(0);
        for(const auto &v:plans){const auto p=v.toObject();const int row=table_->rowCount();table_->insertRow(row);table_->setRowHeight(row,56);
            table_->setItem(row,0,new QTableWidgetItem(QString("%1 / v%2").arg(p.value("level").toString()).arg(p.value("version").toInt())));
            table_->setItem(row,1,new QTableWidgetItem(QString("%1个月 · %2").arg(p.value("months").toInt()).arg(p.value("recurring").toInt()?QStringLiteral("连续订阅"):QStringLiteral("固定期限"))));
            auto *price=new QDoubleSpinBox(table_);price->setRange(.01,1000000);price->setDecimals(2);price->setValue(p.value("price_cent").toInteger()/100.0);table_->setCellWidget(row,2,price);
            auto *discount=new QDoubleSpinBox(table_);discount->setRange(.001,9.999);discount->setDecimals(3);discount->setSuffix(QStringLiteral(" 折"));discount->setValue(p.value("discount_bps").toInt()/1000.0);table_->setCellWidget(row,3,discount);
            price->setButtonSymbols(QAbstractSpinBox::NoButtons);
            discount->setButtonSymbols(QAbstractSpinBox::NoButtons);
            auto *active=new QCheckBox(QStringLiteral("上架"),table_);active->setChecked(p.value("active").toInt());table_->setCellWidget(row,4,active);
            auto *save=new QPushButton(QStringLiteral("保存"),table_);save->setObjectName(QString("savePlan%1").arg(p.value("plan_id").toInt()));save->setProperty("uiClass","secondary");table_->setCellWidget(row,5,save);
            connect(save,&QPushButton::clicked,this,[this,p,price,discount,active]{
                if(busy_)return;
                if(QMessageBox::question(this,QStringLiteral("确认套餐变更"),QStringLiteral("保存将生成新版本；已有自动续费暂停等待用户重新确认。是否继续？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
                const QJsonObject params{{"plan_id",p.value("plan_id")},{"version",p.value("version")},{"price_cent",qint64(std::llround(price->value()*100))},{"discount_bps",int(std::llround(discount->value()*1000))},{"active",active->isChecked()}};
                busy_=true;table_->setEnabled(false);
                api_->sendAction("membership_update",params,this,[this](bool success,const QJsonObject &,const QString &message){busy_=false;table_->setEnabled(true);notice_->setText(success?QStringLiteral("套餐已保存，历史权益和订单优惠不变。"):message);reload();});
            });
        }
    });
}
AdminKnowledgePage::AdminKnowledgePage(AdminApiClient *api,QWidget *parent):QWidget(parent),api_(api){
    auto *l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(12);
    auto *top=new QHBoxLayout;top->addWidget(heading(QStringLiteral("咨询知识库"),this));top->addStretch();auto *create=new QPushButton(QStringLiteral("新增草稿"),this);auto *refresh=new QPushButton(QStringLiteral("刷新"),this);top->addWidget(create);top->addWidget(refresh);l->addLayout(top);
    notice_=heading(QStringLiteral("保存草稿不影响当前答复。审核后发布完整版本；停用后不再用于新答复。"),this,"muted");notice_->setObjectName("knowledgeStatus");l->addWidget(notice_);
    auto *split=new QSplitter(this);list_=new QListWidget(split);list_->setObjectName("knowledgeArticles");list_->setMinimumWidth(170);list_->setMaximumWidth(250);
    auto *form=new QWidget(split);auto *fl=new QVBoxLayout(form);title_=new QLineEdit(form);title_->setObjectName("knowledgeTitle");title_->setMaxLength(120);title_->setPlaceholderText(QStringLiteral("标题"));fl->addWidget(title_);
    keywords_=new QLineEdit(form);keywords_->setObjectName("knowledgeKeywords");keywords_->setMaxLength(500);keywords_->setPlaceholderText(QStringLiteral("检索关键词，用逗号分隔，每词至少2字，如：会员,续费,VIP"));fl->addWidget(keywords_);
    source_=new QLineEdit(form);source_->setObjectName("knowledgeSource");source_->setMaxLength(500);source_->setPlaceholderText(QStringLiteral("可信来源（文档章节或来源网址）"));fl->addWidget(source_);
    content_=new QPlainTextEdit(form);content_->setObjectName("knowledgeContent");content_->setPlaceholderText(QStringLiteral("正文（最多12000字），请勿录入用户个人资料、密码或API Key"));fl->addWidget(content_,1);
    auto *actions=new QHBoxLayout;
    for(const auto &pair:QList<QPair<QString,QString>>{{"knowledge_save",QStringLiteral("保存草稿")},{"knowledge_publish",QStringLiteral("审核并发布")},{"knowledge_disable",QStringLiteral("停用")}}){auto *b=new QPushButton(pair.second,form);b->setObjectName(pair.first);b->setProperty("uiClass",pair.first=="knowledge_publish"?"primary":"secondary");actions->addWidget(b);connect(b,&QPushButton::clicked,this,[this,pair]{action(pair.first);});}
    fl->addLayout(actions);split->addWidget(list_);split->addWidget(form);split->setStretchFactor(1,1);l->addWidget(split,1);
    connect(refresh,&QPushButton::clicked,this,&AdminKnowledgePage::reload);
    connect(create,&QPushButton::clicked,this,[this]{if(!busy_){list_->setCurrentRow(-1);select(-1);}});
    connect(list_,&QListWidget::currentRowChanged,this,&AdminKnowledgePage::select);
}
void AdminKnowledgePage::reload(){
    if(busy_)return;
    busy_=true;
    const qint64 selected=selected_.value("article_id").toInteger();
    api_->sendQuery("knowledge_list",{},this,[this,selected](bool ok,const QJsonObject &r,const QString &message){busy_=false;if(!ok){notice_->setText(message);return;}articles_=r.value("articles").toArray();list_->blockSignals(true);list_->clear();int chosen=-1;
        for(int i=0;i<articles_.size();++i){const auto a=articles_[i].toObject();list_->addItem(a.value("title").toString()+QString("\n%1 · 草稿v%2 / 发布v%3").arg(a.value("active").toInt()?QStringLiteral("已发布"):QStringLiteral("未发布/停用")).arg(a.value("draft_version").toInt()).arg(a.value("published_version").toInt()));if(a.value("article_id").toInteger()==selected)chosen=i;}
        list_->blockSignals(false);if(chosen<0 && !articles_.isEmpty())chosen=0;list_->setCurrentRow(chosen);select(chosen);
    });
}
void AdminKnowledgePage::select(int row){
    selected_=(row>=0 && row<articles_.size())?articles_[row].toObject():QJsonObject{};
    title_->setText(selected_.value("title").toString());keywords_->setText(selected_.value("keywords").toString());source_->setText(selected_.value("source").toString());content_->setPlainText(selected_.value("content").toString());
}
void AdminKnowledgePage::action(const QString &type){
    if(busy_)return;
    const bool dirty=title_->text()!=selected_.value("title").toString() || keywords_->text()!=selected_.value("keywords").toString() || source_->text()!=selected_.value("source").toString() || content_->toPlainText()!=selected_.value("content").toString();
    if(type!="knowledge_save" && (selected_.isEmpty() || dirty)){notice_->setText(QStringLiteral("请先保存草稿，再审核发布或停用。"));return;}
    if(type!="knowledge_save" && QMessageBox::question(this,QStringLiteral("确认知识变更"),type=="knowledge_publish"?QStringLiteral("已核对正文及来源，确认发布此版本供AI使用？"):QStringLiteral("确认停用？新答复将不再使用此知识。"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    QJsonObject p{{"article_id",selected_.value("article_id").toInteger()},{"draft_version",selected_.value("draft_version").toInt()},{"title",title_->text()},{"keywords",keywords_->text()},{"source",source_->text()},{"content",content_->toPlainText()}};
    busy_=true;setEnabled(false);
    api_->sendAction(type,p,this,[this](bool ok,const QJsonObject &r,const QString &message){busy_=false;setEnabled(true);if(ok && r.contains("article_id"))selected_.insert("article_id",r.value("article_id"));notice_->setText(ok?QStringLiteral("操作成功。未发布草稿不会进入AI答复。"):message);if(ok)reload();});
}
}
