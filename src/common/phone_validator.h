#pragma once

#include <QString>
#include <QStringView>

namespace ev {

enum class PhoneValidationError {
    None,
    Empty,
    LengthIncorrect,
    InvalidFormat
};

struct PhoneValidationResult {
    PhoneValidationError error = PhoneValidationError::InvalidFormat;

    bool isValid() const
    {
        return error == PhoneValidationError::None;
    }
};

class PhoneValidator final {
public:
    static PhoneValidationResult validate(QStringView phone);
    static QString errorMessage(PhoneValidationError error);
};

} // namespace ev
