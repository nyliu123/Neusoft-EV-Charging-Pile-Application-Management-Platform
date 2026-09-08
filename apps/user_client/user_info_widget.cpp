#include "user_info_widget.h"
#include "membership_dialog.h"
#include "client_ui/apple_widgets.h"

#include "user_api_client.h"
#include "user_session_state.h"

#include <QBuffer>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QFrame *createCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setProperty("uiClass", "card");
    return card;
}

void refreshStyle(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

qint64 parseAmountCent(const QString &text, bool *ok)
{
    static const QRegularExpression rule(QStringLiteral("^([0-9]+)(?:\\.([0-9]{1,2}))?$"));
    const auto match = rule.match(text);
    if (!match.hasMatch()) {
        *ok = false;
        return 0;
    }
    bool yuanOk = false;
    const qint64 yuan = match.captured(1).toLongLong(&yuanOk);
    QString fraction = match.captured(2);
    if (fraction.size() == 1) {
        fraction.append(QLatin1Char('0'));
    }
    const qint64 cents = fraction.isEmpty() ? 0 : fraction.toLongLong();
    *ok = yuanOk && yuan <= 9999;
    return *ok ? yuan * 100 + cents : 0;
}

} // namespace

UserInfoWidget::UserInfoWidget(ev::UserApiClient *api, QWidget *parent)
    : QWidget(parent), api_(api)
{
    setObjectName(QStringLiteral("userInfoRoot"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(0, 8, 0, 0);
    mainLayout->setSpacing(18);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("个人信息"), this);
    title->setProperty("uiClass", "pageTitle");
    userIdLabel_ = new QLabel(this);
    userIdLabel_->setProperty("uiClass", "muted");
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setProperty("uiClass", "secondary");
    header->addWidget(title);
    header->addSpacing(10);
    header->addWidget(userIdLabel_);
    header->addStretch();
    header->addWidget(refreshButton_);
    mainLayout->addLayout(header);

    auto *profileCard = createCard(this);
    auto *profileLayout = new QHBoxLayout(profileCard);
    profileLayout->setContentsMargins(26, 24, 26, 24);
    profileLayout->setSpacing(20);
    avatarLabel_ = new QLabel(profileCard);
    avatarLabel_->setObjectName(QStringLiteral("profileAvatar"));
    avatarLabel_->setFixedSize(96, 96);
    avatarLabel_->setAlignment(Qt::AlignCenter);
    profileLayout->addWidget(avatarLabel_);
    auto *profileText = new QVBoxLayout;
    auto *nameRow = new QHBoxLayout;
    nicknameLabel_ = new QLabel(profileCard);
    nicknameLabel_->setProperty("uiClass", "profileName");
    editNicknameButton_ = new QPushButton(QStringLiteral("编辑昵称"), profileCard);
    editNicknameButton_->setProperty("uiClass", "secondary");
    nameRow->addWidget(nicknameLabel_);
    nameRow->addWidget(editNicknameButton_);
    nameRow->addStretch();
    profileText->addLayout(nameRow);
    auto *hint = new QLabel(QStringLiteral("头像与昵称修改后会同步到服务端"), profileCard);
    hint->setProperty("uiClass", "muted");
    profileText->addWidget(hint);
    changeAvatarButton_ = new QPushButton(QStringLiteral("更换头像"), profileCard);
    changeAvatarButton_->setProperty("uiClass", "secondary");
    changeAvatarButton_->setMaximumWidth(120);
    profileText->addWidget(changeAvatarButton_);
    profileLayout->addLayout(profileText);
    profileLayout->addStretch();
    mainLayout->addWidget(profileCard);

    auto *walletCard = createCard(this);
    auto *walletLayout = new QHBoxLayout(walletCard);
    walletLayout->setContentsMargins(26, 20, 26, 20);
    auto *walletTitle = new QLabel(QStringLiteral("钱包余额"), walletCard);
    walletTitle->setProperty("uiClass", "sectionTitle");
    balanceLabel_ = new QLabel(walletCard);
    balanceLabel_->setProperty("uiClass", "balanceValue");
    rechargeButton_ = new QPushButton(QStringLiteral("充值"), walletCard);
    rechargeButton_->setProperty("uiClass", "primary");
    walletLayout->addWidget(walletTitle);
    walletLayout->addStretch();
    walletLayout->addWidget(balanceLabel_);
    walletLayout->addSpacing(14);
    walletLayout->addWidget(rechargeButton_);
    mainLayout->addWidget(walletCard);

    auto *operationCard = createCard(this);
    auto *operationLayout = new QHBoxLayout(operationCard);
    operationLayout->setContentsMargins(26, 18, 26, 18);
    auto *operationHint = new QLabel(
        QStringLiteral("会员权益 · VIP充电优惠 / SVIP专属AI咨询"), operationCard);
    operationHint->setWordWrap(true);
    operationHint->setProperty("uiClass", "muted");
    operationLayout->addWidget(operationHint);
    auto *membership = new QPushButton(QStringLiteral("会员中心"), operationCard);
    membership->setObjectName("membershipEntry");membership->setProperty("uiClass","primary");
    auto *consult = new QPushButton(QStringLiteral("AI咨询"), operationCard);
    consult->setObjectName("consultEntry");consult->setProperty("uiClass","secondary");
    operationLayout->addWidget(membership);operationLayout->addWidget(consult);
    const auto openMembership = [this](bool ai) {
        auto *dialog = new MembershipDialog(api_,ai,this);
        connect(dialog,&QDialog::finished,this,[this]{refreshFromServer();});
        dialog->show();
    };
    connect(membership,&QPushButton::clicked,this,[openMembership]{openMembership(false);});
    connect(consult,&QPushButton::clicked,this,[openMembership]{openMembership(true);});
    mainLayout->addWidget(operationCard);
    mainLayout->addStretch();

    connect(refreshButton_, &QPushButton::clicked, this, &UserInfoWidget::refreshFromServer);
    connect(changeAvatarButton_, &QPushButton::clicked, this, &UserInfoWidget::changeAvatar);
    connect(editNicknameButton_, &QPushButton::clicked, this, &UserInfoWidget::editNickname);
    connect(rechargeButton_, &QPushButton::clicked, this, &UserInfoWidget::recharge);
    refreshFromSession();
}

void UserInfoWidget::refreshFromSession()
{
    const UserSessionState &session = UserSessionState::instance();
    const bool loggedIn = session.isLoggedIn();
    userIdLabel_->setText(loggedIn
        ? QStringLiteral("用户ID  %1").arg(session.userId()) : QStringLiteral("未登录"));
    nicknameLabel_->setText(!loggedIn ? QStringLiteral("未登录用户")
        : session.nickname().isEmpty() ? QStringLiteral("未设置昵称") : session.nickname());
    balanceLabel_->setText(QStringLiteral("¥%1")
        .arg(static_cast<double>(session.balanceCent()) / 100.0, 0, 'f', 2));

    QPixmap avatar(session.avatarPath());
    if (avatar.isNull()) {
        // A vector fallback also works on Qt installations without the SVG image plugin.
        avatarLabel_->setText({});
        avatarLabel_->setPixmap(ev::appSymbolIcon(ev::AppSymbol::Person).pixmap(56, 56));
        avatarLabel_->setAccessibleName(QStringLiteral("默认头像"));
        avatarLabel_->setProperty("empty", true);
    } else {
        avatarLabel_->setText({});
        avatarLabel_->setAccessibleName(QStringLiteral("用户头像"));
        avatarLabel_->setProperty("empty", false);
        avatarLabel_->setPixmap(avatar.scaled(avatarLabel_->size(),
            Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    refreshStyle(avatarLabel_);
    setBusy(false);
}

void UserInfoWidget::refreshFromServer()
{
    setBusy(true);
    if (!api_->queryUserInfo(this, [this](bool success, const QJsonObject &result,
                                    const QString &message) {
        setBusy(false);
        if (!success) {
            QMessageBox::warning(this, QStringLiteral("刷新失败"),
                message.isEmpty() ? QStringLiteral("刷新失败，显示的是缓存数据") : message);
            return;
        }
        UserSessionState::instance().updateUserInfo(result);
        refreshFromSession();
    })) {
        setBusy(false);
    }
}

void UserInfoWidget::changeAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), {}, QStringLiteral("图片文件 (*.jpg *.jpeg *.png)"));
    if (path.isEmpty()) {
        return;
    }
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("头像更换失败"),
                             QStringLiteral("图片文件损坏或无法读取"));
        return;
    }
    image = image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPG", 92)) {
        QMessageBox::warning(this, QStringLiteral("头像更换失败"),
                             QStringLiteral("图片转换失败"));
        return;
    }
    setBusy(true);
    if (!api_->updateAvatar(jpegData, this,
        [this](bool success, const QJsonObject &result, const QString &message) {
            setBusy(false);
            if (!success) {
                QMessageBox::warning(this, QStringLiteral("头像更换失败"), message);
                return;
            }
            UserSessionState::instance().setAvatarPath(
                result.value(QStringLiteral("avatar_path")).toString());
            refreshFromSession();
            QMessageBox::information(this, QStringLiteral("更换成功"), message);
        })) {
        setBusy(false);
    }
}

void UserInfoWidget::editNickname()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改昵称"));
    dialog.setMinimumWidth(400);
    auto *layout = new QVBoxLayout(&dialog);
    auto *edit = new QLineEdit(UserSessionState::instance().nickname(), &dialog);
    edit->setMaxLength(20);
    edit->setPlaceholderText(QStringLiteral("1~20个字符：中文、英文、数字、下划线"));
    edit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9_\\x{4E00}-\\x{9FFF}]{0,20}$")), edit));
    auto *errorLabel = new QLabel(&dialog);
    errorLabel->setProperty("uiClass", "errorText");
    auto *buttons = new QHBoxLayout;
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dialog);
    cancel->setProperty("uiClass", "secondary");
    auto *save = new QPushButton(QStringLiteral("保存"), &dialog);
    save->setProperty("uiClass", "primary");
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addWidget(edit);
    layout->addWidget(errorLabel);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dialog, [&, edit, errorLabel, save] {
        const QString nickname = edit->text().trimmed();
        if (nickname.isEmpty()) {
            errorLabel->setText(QStringLiteral("昵称不能为空"));
            return;
        }
        save->setEnabled(false);
        if (!api_->updateNickname(nickname, &dialog,
            [&, nickname, errorLabel, save](bool success, const QJsonObject &result,
                                            const QString &message) {
                save->setEnabled(true);
                if (!success) {
                    errorLabel->setText(message);
                    return;
                }
                UserSessionState::instance().setNickname(
                    result.value(QStringLiteral("nickname")).toString(nickname));
                dialog.accept();
            })) {
            dialog.reject();
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        refreshFromSession();
        QMessageBox::information(this, QStringLiteral("修改成功"),
                                 QStringLiteral("昵称修改成功"));
    }
}

void UserInfoWidget::recharge()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("钱包充值"));
    dialog.setMinimumWidth(410);
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(QStringLiteral("模拟支付：充值范围 ¥0.01 ~ ¥9999.99"), &dialog);
    auto *edit = new QLineEdit(&dialog);
    edit->setPlaceholderText(QStringLiteral("例如 100 或 100.50"));
    auto *errorLabel = new QLabel(&dialog);
    errorLabel->setProperty("uiClass", "errorText");
    auto *buttons = new QHBoxLayout;
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dialog);
    cancel->setProperty("uiClass", "secondary");
    auto *confirm = new QPushButton(QStringLiteral("确认充值"), &dialog);
    confirm->setProperty("uiClass", "primary");
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(confirm);
    layout->addWidget(hint);
    layout->addWidget(edit);
    layout->addWidget(errorLabel);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, &dialog, [&, edit, errorLabel, confirm] {
        bool ok = false;
        const qint64 amountCent = parseAmountCent(edit->text().trimmed(), &ok);
        if (!ok || amountCent < 1 || amountCent > 999999) {
            errorLabel->setText(QStringLiteral("充值金额需在0.01~9999.99元之间，最多2位小数"));
            return;
        }
        confirm->setEnabled(false);
        if (!api_->recharge(amountCent, &dialog,
            [&, errorLabel, confirm](bool success, const QJsonObject &result,
                                     const QString &message) {
                confirm->setEnabled(true);
                if (!success) {
                    errorLabel->setText(message);
                    return;
                }
                UserSessionState::instance().setBalanceCent(
                    result.value(QStringLiteral("balance_cent")).toInteger());
                dialog.accept();
            })) {
            dialog.reject();
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        refreshFromSession();
        QMessageBox::information(this, QStringLiteral("充值成功"),
            QStringLiteral("充值成功，当前余额 ¥%1")
                .arg(static_cast<double>(UserSessionState::instance().balanceCent()) / 100.0,
                     0, 'f', 2));
    }
}

void UserInfoWidget::setBusy(bool busy)
{
    const bool enabled = !busy && UserSessionState::instance().isLoggedIn();
    refreshButton_->setEnabled(enabled);
    changeAvatarButton_->setEnabled(enabled);
    editNicknameButton_->setEnabled(enabled);
    rechargeButton_->setEnabled(enabled);
}
