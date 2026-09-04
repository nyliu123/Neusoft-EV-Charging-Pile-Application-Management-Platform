#include "protocol.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>
#include <QtEndian>

namespace evcs::protocol {

QByteArray encodeFrame(const QJsonObject &message)
{
    // 矩阵约定的传输帧：“4 字节消息类型 + 4 字节大端长度 + UTF-8 JSON”。
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    const QString typeName = message.value(QStringLiteral("type")).toString();
    const MessageType messageType = typeName == QStringLiteral("request")
        ? MessageType::Request
        : (typeName == QStringLiteral("event") ? MessageType::Event : MessageType::Response);
    QByteArray frame;
    frame.reserve(8 + payload.size());

    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(messageType);
    stream << static_cast<quint32>(payload.size());
    frame.append(payload);
    return frame;
}

QJsonObject makeRequest(const QString &action,
                        const QJsonObject &payload,
                        const QString &token,
                        const QString &requestId)
{
    QJsonObject message{
        {QStringLiteral("type"), QStringLiteral("request")},
        {QStringLiteral("requestId"), requestId.isEmpty()
             ? QUuid::createUuid().toString(QUuid::WithoutBraces)
             : requestId},
        {QStringLiteral("action"), action},
        {QStringLiteral("payload"), payload}
    };
    if (!token.isEmpty()) {
        message.insert(QStringLiteral("token"), token);
    }
    return message;
}

QJsonObject makeSuccessResponse(const QString &requestId, const QJsonObject &data)
{
    return {
        {QStringLiteral("type"), QStringLiteral("response")},
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("ok"), true},
        {QStringLiteral("data"), data}
    };
}

QJsonObject makeErrorResponse(const QString &requestId,
                              const QString &code,
                              const QString &message)
{
    return {
        {QStringLiteral("type"), QStringLiteral("response")},
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QJsonObject{
             {QStringLiteral("code"), code},
             {QStringLiteral("message"), message}
         }}
    };
}

QList<QJsonObject> FrameDecoder::append(const QByteArray &bytes, QString *errorMessage)
{
    // 缓冲不完整帧并循环提取完整消息，同时限制单帧最大为 1 MiB。
    if (errorMessage) {
        errorMessage->clear();
    }
    buffer_.append(bytes);
    QList<QJsonObject> messages;

    while (buffer_.size() >= 8) {
        const auto *header = reinterpret_cast<const uchar *>(buffer_.constData());
        const quint32 rawMessageType = qFromBigEndian<quint32>(header);
        const quint32 payloadSize = qFromBigEndian<quint32>(header + 4);
        if (rawMessageType < static_cast<quint32>(MessageType::Request)
            || rawMessageType > static_cast<quint32>(MessageType::Event)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("未知消息类型：%1").arg(rawMessageType);
            }
            reset();
            return {};
        }
        if (payloadSize > MaximumPayloadBytes) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("消息长度超过 1 MiB 限制");
            }
            reset();
            return {};
        }

        const qsizetype frameSize = 8 + static_cast<qsizetype>(payloadSize);
        if (buffer_.size() < frameSize) {
            break;
        }

        const QByteArray payload = buffer_.mid(8, payloadSize);
        buffer_.remove(0, frameSize);

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无效 JSON 消息：%1").arg(parseError.errorString());
            }
            reset();
            return {};
        }
        QJsonObject object = document.object();
        const QString expectedType = rawMessageType == static_cast<quint32>(MessageType::Request)
            ? QStringLiteral("request")
            : (rawMessageType == static_cast<quint32>(MessageType::Event)
                   ? QStringLiteral("event") : QStringLiteral("response"));
        if (object.value(QStringLiteral("type")).toString() != expectedType) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("消息头类型与 JSON type 不一致");
            }
            reset();
            return {};
        }
        messages.append(object);
    }

    return messages;
}

void FrameDecoder::reset()
{
    buffer_.clear();
}

} // namespace evcs::protocol
