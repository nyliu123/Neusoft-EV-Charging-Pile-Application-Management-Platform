#include "style_loader.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>

QString loadStyleResources(const QStringList &resourcePaths)
{
    QString combined;
    for (const QString &path : resourcePaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning().noquote() << QStringLiteral("无法读取样式资源：%1").arg(path);
            continue;
        }
        QTextStream stream(&file);
        combined += stream.readAll();
        combined += QLatin1Char('\n');
    }
    return combined;
}
