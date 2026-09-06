#include "services/user_service.h"

#include "common/phone_validator.h"
#include "data/user_repository.h"

#include <QStringView>
#include <QRegularExpression>
#include <QUuid>
#include <utility>

namespace ev {

namespace {

Result<LoginUserInfo> createSession(const UserRecord &user, bool isNewUser)
{
    if (user.status == QStringLiteral("frozen")) {
        return Result<LoginUserInfo>::fail(
            ErrorCode::AccountFrozen,
            QStringLiteral("账号已被冻结，请联系管理员"));
    }
    if (user.status != QStringLiteral("normal")) {
        return Result<LoginUserInfo>::fail(ErrorCode::Forbidden,
                                            QStringLiteral("账号状态异常，请联系管理员"));
    }

    LoginUserInfo info;
    info.userId = user.userId;
    info.nickname = user.nickname;
    info.avatarPath = user.avatarPath;
    info.balanceCent = user.balanceCent;
    info.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.isNewUser = isNewUser;
    return Result<LoginUserInfo>::ok(std::move(info));
}

} // namespace

Result<LoginUserInfo> UserService::loginExistingUser(QSqlDatabase &database,
                                                      const QString &phone) const
{
    const PhoneValidationResult validation = PhoneValidator::validate(QStringView(phone));
    if (!validation.isValid()) {
        return Result<LoginUserInfo>::fail(ErrorCode::InvalidInput,
                                            PhoneValidator::errorMessage(validation.error));
    }

    const UserRepository repository;
    auto queryResult = repository.findByPhone(database, phone);
    if (!queryResult.success) {
        return Result<LoginUserInfo>::fail(queryResult.code, queryResult.message);
    }
    if (!queryResult.data.has_value()) {
        return Result<LoginUserInfo>::fail(ErrorCode::NotFound,
                                            QStringLiteral("user_not_found"));
    }

    return createSession(queryResult.data.value(), false);
}

Result<LoginUserInfo> UserService::registerAutomatically(QSqlDatabase &database,
                                                          const QString &phone) const
{
    const PhoneValidationResult validation = PhoneValidator::validate(QStringView(phone));
    if (!validation.isValid()) {
        return Result<LoginUserInfo>::fail(ErrorCode::InvalidInput,
                                            PhoneValidator::errorMessage(validation.error));
    }

    const QString nickname = QStringLiteral("用户%1").arg(phone.right(4));
    const QString avatarPath = QStringLiteral(":/images/default-avatar.svg");
    const UserRepository repository;
    auto creation = repository.createAutoRegisteredUser(database, phone, nickname, avatarPath);
    if (!creation.success) {
        return Result<LoginUserInfo>::fail(creation.code, creation.message);
    }
    return createSession(creation.data.user, creation.data.created);
}

Result<UserRecord> UserService::queryUserInfo(QSqlDatabase &database, qint64 userId) const
{
    const UserRepository repository;
    auto result = repository.findById(database, userId);
    if (!result.success) {
        return Result<UserRecord>::fail(result.code, result.message);
    }
    if (!result.data.has_value()) {
        return Result<UserRecord>::fail(ErrorCode::NotFound,
                                        QStringLiteral("user_not_found"));
    }
    const UserRecord &user = result.data.value();
    if (user.status == QStringLiteral("frozen")) {
        return Result<UserRecord>::fail(ErrorCode::AccountFrozen,
                                        QStringLiteral("账号已被冻结，请联系管理员"));
    }
    if (user.status != QStringLiteral("normal")) {
        return Result<UserRecord>::fail(ErrorCode::Forbidden,
                                        QStringLiteral("账号状态异常，请联系管理员"));
    }
    return Result<UserRecord>::ok(user);
}

Result<bool> UserService::updateNickname(QSqlDatabase &database, qint64 userId,
                                         const QString &nickname) const
{
    static const QRegularExpression rule(
        QStringLiteral("^[A-Za-z0-9_\\x{4E00}-\\x{9FFF}]{1,20}$"));
    if (!rule.match(nickname).hasMatch()) {
        return Result<bool>::fail(
            ErrorCode::InvalidInput,
            QStringLiteral("昵称只能包含中文、英文、数字和下划线，长度为1~20个字符"));
    }
    auto current = queryUserInfo(database, userId);
    if (!current.success) {
        return Result<bool>::fail(current.code, current.message);
    }
    return UserRepository().updateNickname(database, userId, nickname);
}

Result<bool> UserService::updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                           const QString &avatarPath) const
{
    if (avatarPath.isEmpty()) {
        return Result<bool>::fail(ErrorCode::InvalidInput,
                                  QStringLiteral("头像路径不能为空"));
    }
    auto current = queryUserInfo(database, userId);
    if (!current.success) {
        return Result<bool>::fail(current.code, current.message);
    }
    return UserRepository().updateAvatarPath(database, userId, avatarPath);
}

Result<qint64> UserService::recharge(QSqlDatabase &database, qint64 userId,
                                     qint64 amountCent) const
{
    if (amountCent < 1 || amountCent > 999999) {
        return Result<qint64>::fail(
            ErrorCode::InvalidInput,
            QStringLiteral("充值金额需在0.01~9999.99元之间"));
    }
    auto current = queryUserInfo(database, userId);
    if (!current.success) {
        return Result<qint64>::fail(current.code, current.message);
    }
    return UserRepository().recharge(database, userId, amountCent);
}

} // namespace ev
