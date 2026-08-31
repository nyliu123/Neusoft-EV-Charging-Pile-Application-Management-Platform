#include "logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QThread>

namespace evcs::server {
namespace {

QFile logFile;
QMutex logMutex;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("debug");
    case QtInfoMsg: return QStringLiteral("info");
    case QtWarningMsg: return QStringLiteral("warning");
    case QtCriticalMsg: return QStringLiteral("critical");
    case QtFatalMsg: return QStringLiteral("fatal");
    }
    return QStringLiteral("unknown");
}

void structuredMessageHandler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &message)
{
    const QJsonObject entry{
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("level"), levelName(type)},
        {QStringLiteral("category"), QString::fromUtf8(context.category ? context.category : "default")},
        {QStringLiteral("message"), message},
        {QStringLiteral("processId"), static_cast<double>(QCoreApplication::applicationPid())},
        {QStringLiteral("threadId"), QString::number(
             reinterpret_cast<quintptr>(QThread::currentThreadId()), 16)}
    };
    const QByteArray line = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
    QMutexLocker locker(&logMutex);
    if (logFile.isOpen()) {
        logFile.write(line);
        logFile.flush();
    }
    const bool error = type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg;
    FILE *stream = error ? stderr : stdout;
    fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stream);
    fflush(stream);
    if (type == QtFatalMsg) abort();
}

} // namespace

bool installStructuredLogging(const QString &logPath, QString *errorMessage)
{
    if (!logPath.trimmed().isEmpty()) {
        const QFileInfo info(logPath);
        if (!QDir().mkpath(info.absolutePath())) {
            if (errorMessage) *errorMessage = QStringLiteral("无法创建日志目录：%1").arg(info.absolutePath());
            return false;
        }
        logFile.setFileName(info.absoluteFilePath());
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            if (errorMessage) *errorMessage = QStringLiteral("无法打开日志文件：%1").arg(logFile.errorString());
            return false;
        }
    }
    qInstallMessageHandler(structuredMessageHandler);
    return true;
}

} // namespace evcs::server
