#include "services/user_service.h"

#include "common/phone_validator.h"
#include "data/user_repository.h"

#include <QStringView>
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

} // namespace ev
