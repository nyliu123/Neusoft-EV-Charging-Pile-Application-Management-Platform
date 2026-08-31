#pragma once

#include "protocol.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace evcs {

class ApiClient final : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient() override;

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    QString token() const;
    void setToken(const QString &token);
    void clearToken();

    QString sendRequest(const QString &action, const QJsonObject &payload = {});

signals:
    void connectionChanged(bool connected, const QString &message);
    void responseReceived(const QString &requestId,
                          const QString &action,
                          bool ok,
                          const QJsonObject &data,
                          const QString &errorCode,
                          const QString &errorMessage);

private:
    void readResponses();

    QTcpSocket socket_;
    QTimer requestTimer_;
    protocol::FrameDecoder decoder_;
    QString token_;
    QHash<QString, QString> pendingActions_;
    QHash<QString, qint64> requestDeadlines_;
};

} // namespace evcs
