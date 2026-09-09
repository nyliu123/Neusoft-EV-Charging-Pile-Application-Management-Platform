#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
class QLabel; class QPushButton; class QComboBox; class QGridLayout; class QPlainTextEdit; class QTabWidget; class QListWidget; class QVBoxLayout; class QEvent;
namespace ev { class UserApiClient; }
class MembershipDialog final : public QDialog {
    Q_OBJECT
public:
    explicit MembershipDialog(ev::UserApiClient *api,bool openConsult=false,QWidget *parent=nullptr);
    void reload();
protected:
    void reject() override;
    bool eventFilter(QObject *watched,QEvent *event) override;
private:
    void renderPlans();
    void buy(const QJsonObject &plan);
    void submitPurchase(const QJsonObject &params);
    void ask();
    void sendQuestion(const QString &question, bool appendUser);
    void loadChatSessions();
    void saveChatSessions() const;
    void createChatSession();
    void renderChatSessions();
    void renderChatMessages();
    void selectChatSession(const QString &sessionId);
    void deleteChatSession();
    void deleteChatSessionById(const QString &sessionId);
    int chatSessionIndex(const QString &sessionId) const;
    void copyChatText(const QString &text) const;
    void editUserMessage(int messageIndex);
    void retryMessage(int messageIndex);
    void setBusy(bool busy);
    ev::UserApiClient *api_;
    QLabel *status_, *notice_, *consultHint_;
    QPushButton *send_;
    QComboBox *billing_;
    QGridLayout *vipCards_,*svipCards_;
    QWidget *vipCardContainer_,*svipCardContainer_;
    QTabWidget *planLevelTabs_;
    QPlainTextEdit *chat_, *question_;
    QListWidget *chatSessionList_=nullptr;
    QVBoxLayout *chatMessagesLayout_=nullptr;
    QWidget *chatMessagesContainer_=nullptr;
    QTabWidget *tabs_;
    QJsonArray plans_;
    QJsonObject state_;
    QJsonArray chatSessions_;
    QString currentChatSessionId_;
    bool busy_=false;
    bool consultOnly_=false;
    int editingMessageIndex_=-1;
    int chatGeneration_=0;
};
