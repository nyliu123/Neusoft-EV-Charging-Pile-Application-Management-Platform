
#include <QApplication>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QString>

struct UserInfo
{
    QString userId;
    QString nickname;
    QString avatarPath;
    double balance = 0.0;
    QString status;
};

/*
 * 用户端全局状态。
 *
 * 按 UML-014 的设计，个人信息页面首屏只读取这里的 user_info，
 * 不在界面类里自行构造“禹晨/学校/余额”等初始用户数据。
 *
 * 实际总项目中，这个对象应由 UML-013 登录流程写入。
 */
class UserGlobalState
{
public:
    static UserGlobalState &instance()
    {
        static UserGlobalState state;
        return state;
    }

    const UserInfo &userInfo() const
    {
        return m_userInfo;
    }

    void setUserInfo(const UserInfo &info)
    {
        m_userInfo = info;
    }

    void updateUserInfo(const UserInfo &info)
    {
        m_userInfo = info;
    }

    void clear()
    {
        m_userInfo = UserInfo();
    }

private:
    UserGlobalState() = default;
    UserInfo m_userInfo;
};

/*
 * 用户服务的独立开发模拟层。
 * 真实项目中，这里应替换为与服务端通信的实现。
 * UI 不直接访问数据库。
 */
class UserService
{
public:
    explicit UserService(const UserInfo &serverInfo)
        : m_serverInfo(serverInfo)
    {
    }

    bool queryUserInfo(UserInfo &out, QString &error) const
    {
        if (m_serverInfo.userId.isEmpty())
        {
            error = QString::fromUtf8("当前没有可用的登录用户。");
            return false;
        }

        out = m_serverInfo;
        return true;
    }

    bool updateAvatar(const QString &filePath, QString &newPath, QString &error)
    {
        if (m_serverInfo.userId.isEmpty())
        {
            error = QString::fromUtf8("用户未登录，无法更换头像。");
            return false;
        }

        QFileInfo info(filePath);
        if (!info.exists() || !info.isFile())
        {
            error = QString::fromUtf8("图片文件不存在。");
            return false;
        }

        const QString suffix = info.suffix().toLower();
        if (suffix != "jpg" && suffix != "jpeg" && suffix != "png")
        {
            error = QString::fromUtf8("不支持的图片格式，请选择JPG或PNG。");
            return false;
        }

        QPixmap image(filePath);
        if (image.isNull())
        {
            error = QString::fromUtf8("图片文件损坏或无法读取。");
            return false;
        }

        QDir avatarDir(QDir::currentPath() + "/avatars");
        if (!avatarDir.exists() && !avatarDir.mkpath("."))
        {
            error = QString::fromUtf8("头像保存目录创建失败。");
            return false;
        }

        const QString fileName =
            QString("%1_%2.jpg")
                .arg(m_serverInfo.userId)
                .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));

        newPath = avatarDir.filePath(fileName);

        const QPixmap normalized =
            image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        if (!normalized.save(newPath, "JPG", 92))
        {
            error = QString::fromUtf8("头像上传失败。");
            return false;
        }

        m_serverInfo.avatarPath = newPath;
        return true;
    }

    bool updateNickname(const QString &nickname, QString &error)
    {
        if (m_serverInfo.userId.isEmpty())
        {
            error = QString::fromUtf8("用户未登录，无法修改昵称。");
            return false;
        }

        QRegularExpression rule(
            QString::fromUtf8("^[A-Za-z0-9_\\x{4E00}-\\x{9FFF}]{1,20}$"));

        if (!rule.match(nickname).hasMatch())
        {
            error = QString::fromUtf8(
                "昵称只能包含中文、英文、数字和下划线，长度为1~20个字符。");
            return false;
        }

        m_serverInfo.nickname = nickname;
        return true;
    }

    bool recharge(double amount, double &newBalance, QString &error)
    {
        if (m_serverInfo.userId.isEmpty())
        {
            error = QString::fromUtf8("用户未登录，无法充值。");
            return false;
        }

        if (amount < 0.01 || amount > 9999.99)
        {
            error = QString::fromUtf8("充值金额需在0.01~9999.99元之间。");
            return false;
        }

        m_serverInfo.balance += amount;
        newBalance = m_serverInfo.balance;
        return true;
    }

    UserInfo serverInfo() const
    {
        return m_serverInfo;
    }

private:
    UserInfo m_serverInfo;
};

class AvatarLabel : public QLabel
{
public:
    explicit AvatarLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setFixedSize(96, 96);
        setAlignment(Qt::AlignCenter);
    }

    void setUserInfo(const UserInfo &info)
    {
        m_info = info;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPixmap canvas(size());
        canvas.fill(Qt::transparent);

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath clip;
        clip.addEllipse(2, 2, 92, 92);
        painter.setClipPath(clip);

        QPixmap avatar(m_info.avatarPath);
        if (!m_info.avatarPath.isEmpty() && !avatar.isNull())
        {
            painter.drawPixmap(
                QRect(2, 2, 92, 92),
                avatar.scaled(92, 92, Qt::KeepAspectRatioByExpanding,
                              Qt::SmoothTransformation));
        }
        else
        {
            painter.fillRect(0, 0, width(), height(), QColor("#D9DCE2"));
        }

        painter.end();

        QPainter finalPainter(this);
        finalPainter.drawPixmap(0, 0, canvas);
    }

private:
    UserInfo m_info;
};

class UserInfoWindow : public QWidget
{
public:
    explicit UserInfoWindow(QWidget *parent = nullptr)
        : QWidget(parent),
          m_service(UserGlobalState::instance().userInfo())
    {
        setWindowTitle(QString::fromUtf8("个人信息"));
        resize(760, 650);
        setMinimumSize(680, 580);

        setStyleSheet(
            "QWidget#root { background:#F5F6FA; }"
            "QFrame.card { background:white; border:1px solid #ECEEF3; border-radius:18px; }"
            "QLabel { color:#202124; }"
            "QPushButton { border:none; }"
            "QPushButton.action { background:#F2F6FF; color:#1677FF; "
            "border-radius:10px; padding:8px 15px; font-size:14px; }"
            "QPushButton.action:hover { background:#E5EEFF; }"
            "QPushButton.primary { background:#1677FF; color:white; "
            "border-radius:10px; padding:9px 18px; font-size:14px; }"
            "QPushButton.primary:hover { background:#0D6EEC; }"
            "QPushButton.row { background:white; text-align:left; padding:0 22px; }"
            "QPushButton.row:hover { background:#F8F9FB; }");

        QWidget *root = new QWidget(this);
        root->setObjectName("root");

        QVBoxLayout *mainLayout = new QVBoxLayout(root);
        mainLayout->setContentsMargins(34, 30, 34, 30);
        mainLayout->setSpacing(18);

        QHBoxLayout *header = new QHBoxLayout;

        QLabel *title = new QLabel(QString::fromUtf8("个人信息"));
        title->setStyleSheet("font-size:28px; font-weight:700;");

        QLabel *idLabel = new QLabel;
        idLabel->setStyleSheet("font-size:13px; color:#969CA6;");

        QPushButton *refreshButton = new QPushButton(QString::fromUtf8("刷新"));
        refreshButton->setStyleSheet(
            "QPushButton { background:white; color:#555B66; border:1px solid #E4E7ED; "
            "border-radius:10px; padding:8px 16px; }"
            "QPushButton:hover { background:#F7F8FA; }");

        header->addWidget(title);
        header->addSpacing(12);
        header->addWidget(idLabel);
        header->addStretch();
        header->addWidget(refreshButton);
        mainLayout->addLayout(header);

        QFrame *profileCard = createCard();
        QHBoxLayout *profileLayout = new QHBoxLayout(profileCard);
        profileLayout->setContentsMargins(26, 24, 26, 24);
        profileLayout->setSpacing(20);

        m_avatar = new AvatarLabel;
        profileLayout->addWidget(m_avatar);

        QVBoxLayout *profileText = new QVBoxLayout;
        profileText->setSpacing(8);

        QHBoxLayout *nameRow = new QHBoxLayout;
        m_nickname = new QLabel;
        m_nickname->setStyleSheet("font-size:24px; font-weight:700;");

        QPushButton *editNicknameButton =
            new QPushButton(QString::fromUtf8("✎ 编辑"));
        editNicknameButton->setProperty("class", "action");

        nameRow->addWidget(m_nickname);
        nameRow->addWidget(editNicknameButton);
        nameRow->addStretch();
        profileText->addLayout(nameRow);

        m_status = new QLabel;
        m_status->setStyleSheet("font-size:13px; color:#8D939D;");
        profileText->addWidget(m_status);

        QPushButton *changeAvatarButton =
            new QPushButton(QString::fromUtf8("更换头像"));
        changeAvatarButton->setProperty("class", "action");
        changeAvatarButton->setMaximumWidth(120);
        profileText->addWidget(changeAvatarButton);

        profileLayout->addLayout(profileText);
        profileLayout->addStretch();
        mainLayout->addWidget(profileCard);

        QFrame *walletCard = createCard();
        QHBoxLayout *walletLayout = new QHBoxLayout(walletCard);
        walletLayout->setContentsMargins(26, 20, 26, 20);

        QLabel *walletIcon = new QLabel(QString::fromUtf8("¥"));
        walletIcon->setAlignment(Qt::AlignCenter);
        walletIcon->setFixedSize(46, 46);
        walletIcon->setStyleSheet(
            "background:#EAF3FF; color:#1677FF; border-radius:13px; "
            "font-size:21px; font-weight:bold;");
        walletLayout->addWidget(walletIcon);

        QVBoxLayout *walletText = new QVBoxLayout;
        QLabel *walletTitle = new QLabel(QString::fromUtf8("钱包余额"));
        walletTitle->setStyleSheet("font-size:16px; font-weight:600;");
        QLabel *walletHint =
            new QLabel(QString::fromUtf8("当前账户可用余额"));
        walletHint->setStyleSheet("font-size:12px; color:#9AA0AA;");
        walletText->addWidget(walletTitle);
        walletText->addWidget(walletHint);
        walletLayout->addLayout(walletText);
        walletLayout->addStretch();

        m_balance = new QLabel;
        m_balance->setStyleSheet(
            "font-size:23px; font-weight:700; color:#202124;");
        walletLayout->addWidget(m_balance);

        QPushButton *rechargeButton =
            new QPushButton(QString::fromUtf8("充值"));
        rechargeButton->setProperty("class", "primary");
        walletLayout->addSpacing(14);
        walletLayout->addWidget(rechargeButton);

        mainLayout->addWidget(walletCard);

        QFrame *settingsCard = createCard();
        QVBoxLayout *settingsLayout = new QVBoxLayout(settingsCard);
        settingsLayout->setContentsMargins(0, 0, 0, 0);
        settingsLayout->setSpacing(0);

        QLabel *settingsTitle =
            new QLabel(QString::fromUtf8("个人信息操作"));
        settingsTitle->setStyleSheet(
            "font-size:16px; font-weight:650; padding:18px 22px 12px;");
        settingsLayout->addWidget(settingsTitle);

        QPushButton *syncRow = createRow(
            QString::fromUtf8("↻"),
            QString::fromUtf8("刷新个人信息"),
            QString::fromUtf8("从用户服务获取最新头像、昵称和余额"));

        QPushButton *avatarRow = createRow(
            QString::fromUtf8("◎"),
            QString::fromUtf8("更换头像"),
            QString::fromUtf8("选择JPG、PNG或JPEG图片"));

        QPushButton *nicknameRow = createRow(
            QString::fromUtf8("✎"),
            QString::fromUtf8("修改昵称"),
            QString::fromUtf8("1~20个字符，仅中文、英文、数字和下划线"));

        settingsLayout->addWidget(syncRow);
        settingsLayout->addWidget(separator());
        settingsLayout->addWidget(avatarRow);
        settingsLayout->addWidget(separator());
        settingsLayout->addWidget(nicknameRow);

        mainLayout->addWidget(settingsCard);
        mainLayout->addStretch();

        QVBoxLayout *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(root);

        renderFromGlobalState();

        connect(refreshButton, &QPushButton::clicked,
                this, [this]() { refreshFromService(); });

        connect(syncRow, &QPushButton::clicked,
                this, [this]() { refreshFromService(); });

        connect(changeAvatarButton, &QPushButton::clicked,
                this, [this]() { changeAvatar(); });

        connect(avatarRow, &QPushButton::clicked,
                this, [this]() { changeAvatar(); });

        connect(editNicknameButton, &QPushButton::clicked,
                this, [this]() { editNickname(); });

        connect(nicknameRow, &QPushButton::clicked,
                this, [this]() { editNickname(); });

        connect(rechargeButton, &QPushButton::clicked,
                this, [this]() { recharge(); });
    }

private:
    UserService m_service;
    AvatarLabel *m_avatar = nullptr;
    QLabel *m_nickname = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_balance = nullptr;

    QFrame *createCard()
    {
        QFrame *card = new QFrame;
        card->setProperty("class", "card");
        return card;
    }

    QFrame *separator()
    {
        QFrame *line = new QFrame;
        line->setFixedHeight(1);
        line->setStyleSheet("background:#F0F1F4; border:none;");
        return line;
    }

    QPushButton *createRow(const QString &icon,
                           const QString &title,
                           const QString &hint)
    {
        QPushButton *button = new QPushButton;
        button->setProperty("class", "row");
        button->setMinimumHeight(64);

        QHBoxLayout *layout = new QHBoxLayout(button);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        QLabel *iconLabel = new QLabel(icon);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setFixedSize(38, 38);
        iconLabel->setStyleSheet(
            "background:#F0F6FF; color:#1677FF; border-radius:11px; "
            "font-size:19px; font-weight:bold;");
        layout->addWidget(iconLabel);

        QVBoxLayout *texts = new QVBoxLayout;
        texts->setSpacing(2);

        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size:15px; font-weight:600;");
        QLabel *hintLabel = new QLabel(hint);
        hintLabel->setStyleSheet("font-size:12px; color:#9AA0AA;");

        texts->addWidget(titleLabel);
        texts->addWidget(hintLabel);
        layout->addLayout(texts);
        layout->addStretch();

        QLabel *arrow = new QLabel(QString::fromUtf8("›"));
        arrow->setStyleSheet("font-size:25px; color:#B4B8C0;");
        layout->addWidget(arrow);

        return button;
    }

    void renderFromGlobalState()
    {
        const UserInfo &info = UserGlobalState::instance().userInfo();

        if (info.userId.isEmpty())
        {
            m_nickname->setText(QString::fromUtf8("未登录用户"));
            m_status->setText(QString::fromUtf8("当前没有登录状态，请先完成UML-013登录"));
            m_balance->setText(QString::fromUtf8("¥0.00"));
        }
        else
        {
            m_nickname->setText(info.nickname.isEmpty()
                                    ? QString::fromUtf8("未设置昵称")
                                    : info.nickname);

            m_status->setText(
                QString::fromUtf8("用户ID  %1").arg(info.userId));

            m_balance->setText(
                QString("¥%1").arg(info.balance, 0, 'f', 2));
        }

        m_avatar->setUserInfo(info);
    }

    void refreshFromService()
    {
        UserInfo latest;
        QString error;

        if (!m_service.queryUserInfo(latest, error))
        {
            QMessageBox::warning(
                this,
                QString::fromUtf8("刷新失败"),
                QString::fromUtf8("刷新失败，显示的是缓存数据。\n%1").arg(error));
            return;
        }

        if (latest.status == QString::fromUtf8("frozen"))
        {
            QMessageBox::warning(
                this,
                QString::fromUtf8("账户已冻结"),
                QString::fromUtf8("当前账户已被冻结，请重新登录。"));
            return;
        }

        UserGlobalState::instance().updateUserInfo(latest);
        renderFromGlobalState();

        QMessageBox::information(
            this,
            QString::fromUtf8("刷新成功"),
            QString::fromUtf8("个人信息已更新。"));
    }

    void changeAvatar()
    {
        const QString file = QFileDialog::getOpenFileName(
            this,
            QString::fromUtf8("选择头像"),
            QDir::homePath(),
            QString::fromUtf8("图片文件 (*.jpg *.jpeg *.png)"));

        if (file.isEmpty())
            return;

        QString error;
        QString newPath;

        if (!m_service.updateAvatar(file, newPath, error))
        {
            QMessageBox::warning(
                this, QString::fromUtf8("头像更换失败"), error);
            return;
        }

        UserInfo updated = UserGlobalState::instance().userInfo();
        updated.avatarPath = newPath;
        UserGlobalState::instance().updateUserInfo(updated);
        renderFromGlobalState();

        QMessageBox::information(
            this,
            QString::fromUtf8("更换成功"),
            QString::fromUtf8("头像已更新。"));
    }

    void editNickname()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8("修改昵称"));
        dialog.setModal(true);
        dialog.setMinimumWidth(400);

        QVBoxLayout layout(&dialog);

        QLabel title(QString::fromUtf8("修改昵称"));
        title.setStyleSheet("font-size:18px; font-weight:700;");
        layout.addWidget(&title);

        QLineEdit edit;
        edit.setText(UserGlobalState::instance().userInfo().nickname);
        edit.setMaxLength(20);
        edit.setPlaceholderText(
            QString::fromUtf8("1~20个字符：中文、英文、数字、下划线"));
        edit.setStyleSheet(
            "QLineEdit { border:1px solid #DDE1E8; border-radius:10px; "
            "padding:10px; font-size:14px; }"
            "QLineEdit:focus { border:1px solid #1677FF; }");

        QRegularExpression clientRule(
            QString::fromUtf8("^[A-Za-z0-9_\\x{4E00}-\\x{9FFF}]{0,20}$"));
        edit.setValidator(new QRegularExpressionValidator(clientRule, &edit));
        layout.addWidget(&edit);

        QLabel errorLabel;
        errorLabel.setWordWrap(true);
        errorLabel.setStyleSheet("color:#E5484D; font-size:12px;");
        layout.addWidget(&errorLabel);

        QHBoxLayout buttons;
        QPushButton cancel(QString::fromUtf8("取消"));
        QPushButton save(QString::fromUtf8("保存"));
        save.setProperty("class", "primary");
        buttons.addStretch();
        buttons.addWidget(&cancel);
        buttons.addWidget(&save);
        layout.addLayout(&buttons);

        connect(&cancel, &QPushButton::clicked,
                &dialog, &QDialog::reject);

        connect(&save, &QPushButton::clicked, &dialog, [&]() {
            const QString value = edit.text().trimmed();

            if (value.isEmpty())
            {
                errorLabel.setText(QString::fromUtf8("昵称不能为空"));
                return;
            }

            QString error;
            if (!m_service.updateNickname(value, error))
            {
                errorLabel.setText(error);
                return;
            }

            UserInfo updated = UserGlobalState::instance().userInfo();
            updated.nickname = value;
            UserGlobalState::instance().updateUserInfo(updated);

            dialog.accept();
        });

        if (dialog.exec() == QDialog::Accepted)
        {
            renderFromGlobalState();
            QMessageBox::information(
                this,
                QString::fromUtf8("修改成功"),
                QString::fromUtf8("昵称修改成功"));
        }
    }

    void recharge()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8("钱包充值"));
        dialog.setModal(true);
        dialog.setMinimumWidth(410);

        QVBoxLayout layout(&dialog);

        QLabel title(QString::fromUtf8("钱包充值"));
        title.setStyleSheet("font-size:18px; font-weight:700;");
        layout.addWidget(&title);

        QLabel hint(QString::fromUtf8(
            "模拟支付：点击确认后直接增加余额\n充值范围：¥0.01 ~ ¥9999.99，最多2位小数"));
        hint.setStyleSheet("font-size:12px; color:#9298A3;");
        layout.addWidget(&hint);

        QLineEdit edit;
        edit.setPlaceholderText(QString::fromUtf8("例如 100 或 100.50"));
        edit.setStyleSheet(
            "QLineEdit { border:1px solid #DDE1E8; border-radius:10px; "
            "padding:10px; font-size:15px; }"
            "QLineEdit:focus { border:1px solid #1677FF; }");
        layout.addWidget(&edit);

        QLabel errorLabel;
        errorLabel.setWordWrap(true);
        errorLabel.setStyleSheet("color:#E5484D; font-size:12px;");
        layout.addWidget(&errorLabel);

        QHBoxLayout buttons;
        QPushButton cancel(QString::fromUtf8("取消"));
        QPushButton confirm(QString::fromUtf8("确认充值"));
        confirm.setProperty("class", "primary");
        buttons.addStretch();
        buttons.addWidget(&cancel);
        buttons.addWidget(&confirm);
        layout.addLayout(&buttons);

        connect(&cancel, &QPushButton::clicked,
                &dialog, &QDialog::reject);

        connect(&confirm, &QPushButton::clicked, &dialog, [&]() {
            const QString value = edit.text().trimmed();

            QRegularExpression moneyRule(
                QString::fromUtf8("^([0-9]+)(\\.[0-9]{1,2})?$"));

            if (value.isEmpty())
            {
                errorLabel.setText(QString::fromUtf8("请输入充值金额"));
                return;
            }

            if (!moneyRule.match(value).hasMatch())
            {
                errorLabel.setText(QString::fromUtf8("请输入正确的金额"));
                return;
            }

            bool ok = false;
            const double amount = value.toDouble(&ok);
            if (!ok || amount < 0.01 || amount > 9999.99)
            {
                errorLabel.setText(
                    QString::fromUtf8("充值金额需在0.01~9999.99元之间"));
                return;
            }

            double newBalance = 0.0;
            QString error;

            if (!m_service.recharge(amount, newBalance, error))
            {
                errorLabel.setText(error);
                return;
            }

            UserInfo updated = UserGlobalState::instance().userInfo();
            updated.balance = newBalance;
            UserGlobalState::instance().updateUserInfo(updated);

            dialog.accept();
        });

        if (dialog.exec() == QDialog::Accepted)
        {
            renderFromGlobalState();

            QMessageBox::information(
                this,
                QString::fromUtf8("充值成功"),
                QString::fromUtf8("充值成功，当前余额 ¥%1")
                    .arg(UserGlobalState::instance().userInfo().balance,
                         0, 'f', 2));
        }
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QString::fromUtf8("UserInfoQt"));

    /*
     * 独立验收版本故意不在这里伪造“禹晨/学校/128.50”等用户数据。
     *
     * 实际总项目中：
     * UML-013 登录成功后应写入：
     * UserGlobalState::instance().setUserInfo(loginUserInfo);
     *
     * 本独立模块没有 UML-013，因此首次运行会显示“未登录用户”，
     * 这是符合 UML-014 前置条件的安全状态，而不是虚构登录数据。
     */
    UserInfoWindow window;
    window.show();

    return app.exec();
}
