#pragma once

#include "common/error_code.h"

#include <QByteArray>
#include <QJsonObject>
#include <QtGlobal>

namespace ev {

struct Frame {
    quint32 messageType = 0;
    QJsonObject payload;
};

enum class DecodeStatus {
    Complete,
    NeedMore,
    Invalid
};

struct DecodeResult {
    DecodeStatus status = DecodeStatus::NeedMore;
    Frame frame;
    ErrorCode error = ErrorCode::Ok;
    QString message;
};

class FrameCodec final {
public:
    static QByteArray encode(quint32 messageType, const QJsonObject &payload);
    static DecodeResult decodeOne(QByteArray &buffer);
};

} // namespace ev

