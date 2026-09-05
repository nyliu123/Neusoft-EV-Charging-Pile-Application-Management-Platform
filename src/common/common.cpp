#include "common/error_code.h"

namespace ev {

QString errorCodeName(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Ok: return QStringLiteral("OK");
    case ErrorCode::InvalidInput: return QStringLiteral("INVALID_INPUT");
    case ErrorCode::Unauthorized: return QStringLiteral("UNAUTHORIZED");
    case ErrorCode::Forbidden: return QStringLiteral("FORBIDDEN");
    case ErrorCode::AccountFrozen: return QStringLiteral("ACCOUNT_FROZEN");
    case ErrorCode::NotFound: return QStringLiteral("NOT_FOUND");
    case ErrorCode::StateConflict: return QStringLiteral("STATE_CONFLICT");
    case ErrorCode::RequestConflict: return QStringLiteral("REQUEST_CONFLICT");
    case ErrorCode::InsufficientBalance: return QStringLiteral("INSUFFICIENT_BALANCE");
    case ErrorCode::StorageError: return QStringLiteral("STORAGE_ERROR");
    case ErrorCode::Busy: return QStringLiteral("BUSY");
    case ErrorCode::ProtocolError: return QStringLiteral("PROTOCOL_ERROR");
    case ErrorCode::MapUnavailable: return QStringLiteral("MAP_UNAVAILABLE");
    case ErrorCode::ModelUnavailable: return QStringLiteral("MODEL_UNAVAILABLE");
    case ErrorCode::InternalError: return QStringLiteral("INTERNAL_ERROR");
    }
    return QStringLiteral("INTERNAL_ERROR");
}

} // namespace ev

