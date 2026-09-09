#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
class QLabel; class QTableWidget; class QListWidget; class QLineEdit; class QPlainTextEdit;
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
}
