#include "protocol.h"

#include <QJsonObject>
#include <QtTest>

class ProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripHandlesSplitFrame();
    void rejectsOversizedFrame();
};

void ProtocolTest::roundTripHandlesSplitFrame()
{
    const QJsonObject request = evcs::protocol::makeRequest(
        QStringLiteral("system.ping"), {}, {}, QStringLiteral("request-1"));
    const QByteArray frame = evcs::protocol::encodeFrame(request);

    evcs::protocol::FrameDecoder decoder;
    QString error;
    QCOMPARE(decoder.append(frame.left(3), &error).size(), 0);
    QVERIFY(error.isEmpty());

    const auto messages = decoder.append(frame.mid(3), &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().value(QStringLiteral("action")).toString(),
             QStringLiteral("system.ping"));
    QCOMPARE(messages.first().value(QStringLiteral("requestId")).toString(),
             QStringLiteral("request-1"));
}

void ProtocolTest::rejectsOversizedFrame()
{
    QByteArray frame(8, '\0');
    frame[3] = static_cast<char>(static_cast<quint32>(evcs::protocol::MessageType::Request));
    const quint32 length = evcs::protocol::MaximumPayloadBytes + 1;
    frame[4] = static_cast<char>((length >> 24) & 0xff);
    frame[5] = static_cast<char>((length >> 16) & 0xff);
    frame[6] = static_cast<char>((length >> 8) & 0xff);
    frame[7] = static_cast<char>(length & 0xff);

    evcs::protocol::FrameDecoder decoder;
    QString error;
    QVERIFY(decoder.append(frame, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(ProtocolTest)

#include "protocol_test.moc"
