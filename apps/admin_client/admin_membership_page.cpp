#include "admin_membership_page.h"
#include "admin_api_client.h"
#include "client_ui/eye_line_edit.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
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
    notice_=heading(QString(),this,"muted");notice_->setObjectName("knowledgeStatus");notice_->hide();l->addWidget(notice_);
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
    api_->sendQuery("knowledge_list",{},this,[this,selected](bool ok,const QJsonObject &r,const QString &message){busy_=false;if(!ok){notice_->show();notice_->setText(message);return;}articles_=r.value("articles").toArray();list_->blockSignals(true);list_->clear();int chosen=-1;
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
    if(type!="knowledge_save" && (selected_.isEmpty() || dirty)){notice_->show();notice_->setText(QStringLiteral("请先保存草稿，再审核发布或停用。"));return;}
    if(type!="knowledge_save" && QMessageBox::question(this,QStringLiteral("确认知识变更"),type=="knowledge_publish"?QStringLiteral("已核对正文及来源，确认发布此版本供AI使用？"):QStringLiteral("确认停用？新答复将不再使用此知识。"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    QJsonObject p{{"article_id",selected_.value("article_id").toInteger()},{"draft_version",selected_.value("draft_version").toInt()},{"title",title_->text()},{"keywords",keywords_->text()},{"source",source_->text()},{"content",content_->toPlainText()}};
    busy_=true;setEnabled(false);
    api_->sendAction(type,p,this,[this](bool ok,const QJsonObject &r,const QString &message){busy_=false;setEnabled(true);if(ok && r.contains("article_id"))selected_.insert("article_id",r.value("article_id"));notice_->show();notice_->setText(ok?QStringLiteral("操作成功。"):message);if(ok)reload();});
}

AdminAiSettingsPage::AdminAiSettingsPage(AdminApiClient *api,QWidget *parent):QWidget(parent),api_(api){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(10,14,10,10);layout->setSpacing(14);
    auto *title=heading(QStringLiteral("基础设置"),this,"sectionTitle");layout->addWidget(title);
    notice_=heading(QString(),this,"muted");notice_->setObjectName("aiSettingsStatus");notice_->hide();layout->addWidget(notice_);
    auto *form=new QFormLayout;form->setSpacing(12);
    auto *apiKeyInput=new EyeLineEdit(this);apiKey_=apiKeyInput;apiKey_->setObjectName("aiApiKey");apiKey_->setEchoMode(QLineEdit::Password);apiKey_->setPlaceholderText(QStringLiteral("输入新 API Key；留空则不修改"));
    apiKeyToggle_=apiKeyInput->eyeButton();apiKeyInput->setEyeVisible(false);apiKeyToggle_->setToolTip(QStringLiteral("显示密钥"));
    connect(apiKeyToggle_,&QToolButton::clicked,this,&AdminAiSettingsPage::toggleApiKeyVisibility);
    baseUrl_=new QLineEdit(this);baseUrl_->setObjectName("aiBaseUrl");baseUrl_->setPlaceholderText(QStringLiteral("https://api.moonshot.cn/v1"));
    model_=new QLineEdit(this);model_->setObjectName("aiModel");model_->setPlaceholderText(QStringLiteral("kimi-k2.6"));
    systemPrompt_=new QPlainTextEdit(this);systemPrompt_->setObjectName("aiSystemPrompt");systemPrompt_->setPlaceholderText(QStringLiteral("设置小轻的身份、回答范围和约束"));systemPrompt_->setMinimumHeight(220);
    form->addRow(QStringLiteral("API Key"),apiKey_);form->addRow(QStringLiteral("Base URL"),baseUrl_);form->addRow(QStringLiteral("模型"),model_);form->addRow(QStringLiteral("系统提示词"),systemPrompt_);layout->addLayout(form);layout->addStretch();
    auto *save=new QPushButton(QStringLiteral("保存基础设置"),this);save->setObjectName("saveAiSettings");save->setProperty("uiClass","primary");layout->addWidget(save,0,Qt::AlignRight);
    connect(save,&QPushButton::clicked,this,[this]{if(busy_)return;busy_=true;setEnabled(false);auto *keyInput=static_cast<EyeLineEdit *>(apiKey_);api_->sendAction("ai_settings_update",{{"api_key",keyInput->hasStoredMask()?QString():apiKey_->text()},{"base_url",baseUrl_->text()},{"model",model_->text()},{"system_prompt",systemPrompt_->toPlainText()}},this,[this](bool ok,const QJsonObject &,const QString &message){busy_=false;setEnabled(true);notice_->show();notice_->setText(ok?QStringLiteral("基础设置已保存。"):message);if(ok)reload();});});
}
void AdminAiSettingsPage::toggleApiKeyVisibility(){
    if(busy_)return;
    auto *keyInput=static_cast<EyeLineEdit *>(apiKey_);
    if(apiKeyVisible_){
        apiKeyVisible_=false;apiKey_->setEchoMode(QLineEdit::Password);static_cast<EyeLineEdit *>(apiKey_)->setEyeVisible(false);apiKeyToggle_->setToolTip(QStringLiteral("显示密钥"));
        if(revealedStoredApiKey_){revealedStoredApiKey_=false;keyInput->setStoredMask(true);}
        return;
    }
    if(!keyInput->hasStoredMask()){
        apiKeyVisible_=true;apiKey_->setEchoMode(QLineEdit::Normal);static_cast<EyeLineEdit *>(apiKey_)->setEyeVisible(true);apiKeyToggle_->setToolTip(QStringLiteral("隐藏密钥"));return;
    }
    bool accepted=false;const QString password=QInputDialog::getText(this,QStringLiteral("管理员身份验证"),QStringLiteral("请输入当前管理员密码"),QLineEdit::Password,QString(),&accepted);
    if(!accepted)return;if(password.isEmpty()){notice_->show();notice_->setText(QStringLiteral("请输入管理员密码。"));return;}
    busy_=true;setEnabled(false);
    api_->sendAction("ai_settings_reveal",{{"password",password}},this,[this](bool ok,const QJsonObject &result,const QString &message){
        busy_=false;setEnabled(true);notice_->show();if(!ok){notice_->setText(message);return;}
        auto *input=static_cast<EyeLineEdit *>(apiKey_);input->setStoredMask(false);apiKey_->setText(result.value("api_key").toString());apiKey_->setEchoMode(QLineEdit::Normal);apiKeyVisible_=true;revealedStoredApiKey_=true;input->setEyeVisible(true);apiKeyToggle_->setToolTip(QStringLiteral("隐藏密钥"));notice_->setText(QStringLiteral("管理员身份验证通过。"));
    });
}
void AdminAiSettingsPage::reload(){
    if(busy_)return;busy_=true;
    api_->sendQuery("ai_settings",{},this,[this](bool ok,const QJsonObject &result,const QString &message){busy_=false;if(!ok){notice_->show();notice_->setText(message);return;}baseUrl_->setText(result.value("base_url").toString());model_->setText(result.value("model").toString());systemPrompt_->setPlainText(result.value("system_prompt").toString());hasSavedApiKey_=result.value("has_api_key").toBool();apiKeyVisible_=false;revealedStoredApiKey_=false;auto *input=static_cast<EyeLineEdit *>(apiKey_);input->setEyeVisible(false);input->setStoredMask(hasSavedApiKey_);if(!hasSavedApiKey_){apiKey_->setEchoMode(QLineEdit::Password);apiKey_->setPlaceholderText(QStringLiteral("请输入 API Key"));}apiKeyToggle_->setToolTip(QStringLiteral("显示密钥"));});
}
AdminAiPage::AdminAiPage(AdminApiClient *api,QWidget *parent):QWidget(parent){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(12);layout->addWidget(heading(QStringLiteral("AI助手配置"),this));tabs_=new QTabWidget(this);settings_=new AdminAiSettingsPage(api,tabs_);knowledge_=new AdminKnowledgePage(api,tabs_);tabs_->addTab(settings_,QStringLiteral("基础设置"));tabs_->addTab(knowledge_,QStringLiteral("咨询知识库"));layout->addWidget(tabs_,1);connect(tabs_,&QTabWidget::currentChanged,this,[this](int index){if(index==0)settings_->reload();else knowledge_->reload();});
}
void AdminAiPage::reload(){if(tabs_->currentIndex()==0)settings_->reload();else knowledge_->reload();}
}
