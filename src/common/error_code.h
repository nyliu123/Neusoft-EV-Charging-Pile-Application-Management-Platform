#pragma once

#include <QString>

namespace ev {

enum class ErrorCode {
    Ok,
    InvalidInput,
    Unauthorized,
    Forbidden,
    AccountFrozen,
    NotFound,
    StateConflict,
    RequestConflict,
    InsufficientBalance,
    StorageError,
    Busy,
    ProtocolError,
    MapUnavailable,
    ModelUnavailable,
    InternalError
};

QString errorCodeName(ErrorCode code);

} // namespace ev

