#include "common/phone_validator.h"

#include <QRegularExpression>

namespace ev {

PhoneValidationResult PhoneValidator::validate(QStringView phone)
{
    if (phone.isEmpty()) {
        return {PhoneValidationError::Empty};
    }

    if (phone.size() != 11) {
        return {PhoneValidationError::LengthIncorrect};
    }

    static const QRegularExpression pattern(QStringLiteral("^1[3-9][0-9]{9}$"));
    if (!pattern.match(phone.toString()).hasMatch()) {
        return {PhoneValidationError::InvalidFormat};
    }
    return {PhoneValidationError::None};
}

QString PhoneValidator::errorMessage(PhoneValidationError error)
{
    switch (error) {
    case PhoneValidationError::None:
        return {};
    case PhoneValidationError::Empty:
        return QStringLiteral("请输入手机号");
    case PhoneValidationError::LengthIncorrect:
        return QStringLiteral("请输入11位手机号");
    case PhoneValidationError::InvalidFormat:
        return QStringLiteral("手机号格式不正确，请检查后重新输入");
    }
    return QStringLiteral("手机号格式不正确");
}

} // namespace ev
