#pragma once

#include "common/error_code.h"

#include <QString>
#include <utility>

namespace ev {

template<typename T>
struct Result {
    bool success = false;
    ErrorCode code = ErrorCode::InternalError;
    QString message;
    T data {};

    static Result ok(T value)
    {
        return {true, ErrorCode::Ok, {}, std::move(value)};
    }

    static Result fail(ErrorCode error, QString detail)
    {
        return {false, error, std::move(detail), {}};
    }
};

} // namespace ev

