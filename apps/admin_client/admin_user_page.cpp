#include "admin_user_page.h"

#include "admin_api_client.h"
#include "admin_format.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace ev {

namespace {

constexpr int kColumnUserId = 0;
constexpr int kColumnPhone = 1;
constexpr int kColumnNickname = 2;
constexpr int kColumnBalance = 3;
constexpr int kColumnRegisterTime = 4;
constexpr int kColumnStatus = 5;
constexpr int kColumnAction = 6;

constexpr int kSearchDebounceMs = 300;

} // namespace

AdminUserPage::AdminUserPage(AdminApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // Header + search.
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    auto *titleLabel = new QLabel(QStringLiteral("用户管理"), this);
    titleLabel->setProperty("uiClass", "pageTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto *searchLabel = new QLabel(QStringLiteral("手机号："), this);
    searchLabel->setProperty("uiClass", "formLabel");
    headerLayout->addWidget(searchLabel);
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("输入手机号片段，如 138 / 8000"));
    searchEdit_->setMinimumWidth(240);
    searchEdit_->setClearButtonEnabled(true);
    headerLayout->addWidget(searchEdit_);

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setProperty("uiClass", "primary");
    headerLayout->addWidget(refreshButton_);
    rootLayout->addLayout(headerLayout);

    // Table.
    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("用户ID"),
        QStringLiteral("手机号"),
        QStringLiteral("昵称"),
        QStringLiteral("钱包余额"),
        QStringLiteral("注册时间"),
        QStringLiteral("账号状态"),
        QStringLiteral("操作")
    });
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColumnUserId, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnPhone, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnStatus, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColumnAction, QHeaderView::ResizeToContents);
    rootLayout->addWidget(table_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setProperty("tone", "muted");
    rootLayout->addWidget(statusLabel_);

    // Loading overlay — centred in the table area.
    loadingOverlay_ = new QLabel(table_);
    loadingOverlay_->setAlignment(Qt::AlignCenter);
    loadingOverlay_->setProperty("uiClass", "loadingOverlay");
    loadingOverlay_->hide();

    // Fuzzy search with a 300 ms debounce so typing does not flood the server.
    searchDebounce_ = new QTimer(this);
    searchDebounce_->setSingleShot(true);
    searchDebounce_->setInterval(kSearchDebounceMs);
    connect(searchDebounce_, &QTimer::timeout, this, &AdminUserPage::loadUsers);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        searchDebounce_->start();
    });
    connect(searchEdit_, &QLineEdit::returnPressed, this, [this] {
        searchDebounce_->stop();
        loadUsers();
    });
    connect(refreshButton_, &QPushButton::clicked, this, &AdminUserPage::loadUsers);
}

void AdminUserPage::reload()
{
    searchDebounce_->stop();
    loadUsers();
}

void AdminUserPage::loadUsers()
{
    hasKeyword_ = !searchEdit_->text().trimmed().isEmpty();

    QJsonObject params;
    const QString keyword = searchEdit_->text().trimmed();
    if (!keyword.isEmpty()) {
        params.insert(QStringLiteral("keyword"), keyword);
    }

    setLoading(true, QStringLiteral("正在加载用户列表..."));
    api_->sendQuery(QStringLiteral("user_list"), params, this,
        [this](bool ok, const QJsonObject &result, const QString &message) {
            setLoading(false, QString());
            if (!ok) {
                table_->setRowCount(0);
                setStatusText(QStringLiteral("用户列表加载失败：%1").arg(message), true);
                return;
            }
            fillTable(result.value(QStringLiteral("users")).toArray());
        });
}

void AdminUserPage::fillTable(const QJsonArray &users)
{
    table_->setRowCount(0);

    if (users.isEmpty()) {
        setStatusText(hasKeyword_
            ? QStringLiteral("未找到匹配的用户，请检查手机号片段后重试")
            : QStringLiteral("暂无注册用户"), false);
        return;
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(users.size());

    for (int row = 0; row < users.size(); ++row) {
        const QJsonObject user = users[row].toObject();
        const QString status = user.value(QStringLiteral("status")).toString();
        const QString phone = user.value(QStringLiteral("phone")).toString();
        const QString nickname = user.value(QStringLiteral("nickname")).toString();

        auto *idItem = new QTableWidgetItem(
            QString::number(user.value(QStringLiteral("user_id")).toInt()));
        idItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnUserId, idItem);

        auto *phoneItem = new QTableWidgetItem(maskPhone(phone));
        phoneItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnPhone, phoneItem);

        table_->setItem(row, kColumnNickname,
            new QTableWidgetItem(nickname.isEmpty() ? QStringLiteral("—") : nickname));

        auto *balanceItem = new QTableWidgetItem(QStringLiteral("¥%1").arg(
            formatAmount(user.value(QStringLiteral("balance")).toDouble())));
        balanceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, kColumnBalance, balanceItem);

        auto *timeItem = new QTableWidgetItem(formatDateTimeShort(
            user.value(QStringLiteral("register_time")).toString()));
        timeItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnRegisterTime, timeItem);

        auto *statusItem = new QTableWidgetItem(userStatusText(status));
        statusItem->setForeground(userStatusColor(status));
        statusItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, kColumnStatus, statusItem);

        const long long userId = user.value(QStringLiteral("user_id")).toInteger();
        const bool frozen = status == QStringLiteral("frozen");
        auto *actionButton = new QPushButton(
            frozen ? QStringLiteral("解冻") : QStringLiteral("冻结"), table_);
        actionButton->setProperty("uiClass", frozen ? "compactPrimary" : "danger");
        actionButton->setCursor(Qt::PointingHandCursor);
        connect(actionButton, &QPushButton::clicked, this,
            [this, userId, phone, nickname, frozen] {
                requestSetStatus(userId, phone, nickname,
                                 frozen ? QStringLiteral("normal")
                                        : QStringLiteral("frozen"));
            });
        table_->setCellWidget(row, kColumnAction, actionButton);
    }
    table_->setUpdatesEnabled(true);

    if (hasKeyword_) {
        setStatusText(QStringLiteral("找到 %1 条匹配结果").arg(users.size()), false);
    } else {
        int frozenCount = 0;
        for (const QJsonValue &value : users) {
            if (value.toObject().value(QStringLiteral("status")).toString()
                == QStringLiteral("frozen")) {
                ++frozenCount;
            }
        }
        setStatusText(frozenCount > 0
            ? QStringLiteral("共 %1 位用户　·　已冻结 %2 位").arg(users.size()).arg(frozenCount)
            : QStringLiteral("共 %1 位用户").arg(users.size()), false);
    }
}

void AdminUserPage::requestSetStatus(long long userId, const QString &phone,
                                     const QString &nickname,
                                     const QString &targetStatus)
{
    const bool freezing = targetStatus == QStringLiteral("frozen");
    const auto answer = QMessageBox::warning(this,
        freezing ? QStringLiteral("冻结用户确认") : QStringLiteral("解冻用户确认"),
        freezing
            ? QStringLiteral("确认冻结用户 %1（%2）？\n\n"
                             "冻结后该用户将立即被强制下线且无法登录。")
                  .arg(maskPhone(phone), nickname)
            : QStringLiteral("确认解冻用户 %1（%2）？\n\n"
                             "解冻后该用户可正常登录。")
                  .arg(maskPhone(phone), nickname),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    setLoading(true, QStringLiteral("正在处理..."));
    api_->sendAction(QStringLiteral("set_user_status"),
        QJsonObject {
            {QStringLiteral("user_id"), userId},
            {QStringLiteral("status"), targetStatus}
        }, this,
        [this](bool ok, const QJsonObject &, const QString &message) {
            setLoading(false, QString());
            if (ok) {
                QMessageBox::information(this, QStringLiteral("操作成功"), message);
            } else {
                QMessageBox::warning(this, QStringLiteral("操作失败"), message);
            }
            // Always refresh so the row reflects the current truth.
            loadUsers();
        });
}

void AdminUserPage::setLoading(bool loading, const QString &message)
{
    if (loading) {
        loadingOverlay_->setText(message.isEmpty()
            ? QStringLiteral("加载中...") : message);
        loadingOverlay_->setGeometry(table_->rect());
        loadingOverlay_->raise();
        loadingOverlay_->show();
        table_->setEnabled(false);
        refreshButton_->setEnabled(false);
        searchEdit_->setEnabled(false);
    } else {
        loadingOverlay_->hide();
        table_->setEnabled(true);
        refreshButton_->setEnabled(true);
        searchEdit_->setEnabled(true);
    }
}

void AdminUserPage::setStatusText(const QString &text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("tone", isError ? "error" : "muted");
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

} // namespace ev
