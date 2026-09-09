#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
class QLabel; class QPushButton; class QComboBox; class QGridLayout; class QPlainTextEdit; class QTabWidget;
namespace ev { class UserApiClient; }
class MembershipDialog final : public QDialog {
    Q_OBJECT
public:
    explicit MembershipDialog(ev::UserApiClient *api,bool openConsult=false,QWidget *parent=nullptr);
    void reload();
protected:
    void reject() override;
private:
    void renderPlans();
    void buy(const QJsonObject &plan);
    void submitPurchase(const QJsonObject &params);
    void authorizeRenewal();
    void ask();
    void setBusy(bool busy);
    QString pendingSettingKey() const;
    ev::UserApiClient *api_;
    QLabel *status_, *notice_, *renewalInfo_, *consultHint_;
    QPushButton *cancelRenewal_, *authorize_, *retryPurchase_, *send_;
    QComboBox *billing_, *renewalPlan_;
    QGridLayout *cards_;
    QWidget *cardContainer_;
    QPlainTextEdit *chat_, *question_;
    QTabWidget *tabs_;
    QJsonArray plans_;
    QJsonObject state_, pendingPurchase_;
    bool busy_=false;
    int chatGeneration_=0;
};
