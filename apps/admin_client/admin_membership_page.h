#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
class QLabel; class QTableWidget; class QListWidget; class QLineEdit; class QPlainTextEdit; class QTabWidget; class QToolButton;
namespace ev {
class AdminApiClient;
class AdminMembershipPage final : public QWidget {
    Q_OBJECT
public:
    explicit AdminMembershipPage(AdminApiClient *api,QWidget *parent=nullptr);
    void reload();
private:
    AdminApiClient *api_;
    QLabel *notice_;
    QTableWidget *table_;
    bool busy_=false;
};
class AdminKnowledgePage final : public QWidget {
    Q_OBJECT
public:
    explicit AdminKnowledgePage(AdminApiClient *api,QWidget *parent=nullptr);
    void reload();
private:
    void select(int row);
    void action(const QString &type);
    AdminApiClient *api_;
    QJsonArray articles_;
    QJsonObject selected_;
    QListWidget *list_;
    QLineEdit *title_, *keywords_, *source_;
    QPlainTextEdit *content_;
    QLabel *notice_;
    bool busy_=false;
};
class AdminAiSettingsPage final : public QWidget {
    Q_OBJECT
public:
    explicit AdminAiSettingsPage(AdminApiClient *api,QWidget *parent=nullptr);
    void reload();
private:
    void toggleApiKeyVisibility();
    AdminApiClient *api_;
    QLineEdit *apiKey_,*baseUrl_,*model_;
    QPlainTextEdit *systemPrompt_;
    QLabel *notice_;
    QToolButton *apiKeyToggle_=nullptr;
    bool busy_=false;
    bool hasSavedApiKey_=false;
    bool apiKeyVisible_=false;
    bool revealedStoredApiKey_=false;
};
class AdminAiPage final : public QWidget {
    Q_OBJECT
public:
    explicit AdminAiPage(AdminApiClient *api,QWidget *parent=nullptr);
    void reload();
private:
    QTabWidget *tabs_;
    AdminAiSettingsPage *settings_;
    AdminKnowledgePage *knowledge_;
};
}
