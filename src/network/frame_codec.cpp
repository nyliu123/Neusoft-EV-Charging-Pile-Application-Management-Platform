#include "network/frame_codec.h"

#include "common/protocol.h"

#include <QJsonDocument>
#include <QtEndian>

namespace ev {

QByteArray FrameCodec::encode(quint32 messageType, const QJsonObject &payload)
{
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (body.size() > static_cast<qsizetype>(MaxPayloadBytes)) {
        return {};
    }

    QByteArray frame(FrameHeaderBytes, Qt::Uninitialized);
    qToBigEndian(messageType, frame.data());
    qToBigEndian(static_cast<quint32>(body.size()), frame.data() + sizeof(quint32));
    frame.append(body);
    return frame;
}

DecodeResult FrameCodec::decodeOne(QByteArray &buffer)
{
    if (buffer.size() < FrameHeaderBytes) {
        return {};
    }

    const auto *bytes = reinterpret_cast<const uchar *>(buffer.constData());
    const quint32 messageType = qFromBigEndian<quint32>(bytes);
    const quint32 payloadLength = qFromBigEndian<quint32>(bytes + sizeof(quint32));
    if (payloadLength > MaxPayloadBytes) {
        return {DecodeStatus::Invalid, {}, ErrorCode::ProtocolError,
                QStringLiteral("payload exceeds configured limit")};
    }

    const qsizetype frameLength = FrameHeaderBytes + static_cast<qsizetype>(payloadLength);
    if (buffer.size() < frameLength) {
        return {};
    }

    const QByteArray body = buffer.mid(FrameHeaderBytes, payloadLength);
    buffer.remove(0, frameLength);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {DecodeStatus::Invalid, {}, ErrorCode::ProtocolError,
                QStringLiteral("payload is not a JSON object")};
    }

    return {DecodeStatus::Complete, {messageType, document.object()}, ErrorCode::Ok, {}};
}

} // namespace ev

