#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace evcs::protocol {

inline constexpr quint32 MaximumPayloadBytes = 1024U * 1024U;

enum class MessageType : quint32 {
    Request = 1,
    Response = 2,
    Event = 3
};

QByteArray encodeFrame(const QJsonObject &message);

QJsonObject makeRequest(const QString &action,
                        const QJsonObject &payload = {},
                        const QString &token = {},
                        const QString &requestId = {});

QJsonObject makeSuccessResponse(const QString &requestId,
                                const QJsonObject &data = {});

QJsonObject makeErrorResponse(const QString &requestId,
                              const QString &code,
                              const QString &message);

class FrameDecoder
{
public:
    QList<QJsonObject> append(const QByteArray &bytes, QString *errorMessage = nullptr);
    void reset();

private:
    QByteArray buffer_;
};

} // namespace evcs::protocol
