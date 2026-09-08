#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QJsonArray;
class QPushButton;
class QTableWidget;
class QTimer;

namespace ev {

class AdminApiClient;

// UML-043~045: user list with masked phones, fuzzy phone search (300 ms
// debounce) and freeze / unfreeze actions. Freezing a user immediately kills
// their live sessions server-side.
class AdminUserPage final : public QWidget {
    Q_OBJECT

public:
    explicit AdminUserPage(AdminApiClient *api, QWidget *parent = nullptr);

    void reload();

private:
    void loadUsers();
    void fillTable(const QJsonArray &users);
    void requestSetStatus(long long userId, const QString &phone,
                          const QString &nickname, const QString &targetStatus);
    void setLoading(bool loading, const QString &message);
    void setStatusText(const QString &text, bool isError);

    AdminApiClient *api_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *loadingOverlay_ = nullptr;
    QTimer *searchDebounce_ = nullptr;
    bool hasKeyword_ = false;
};

} // namespace ev
