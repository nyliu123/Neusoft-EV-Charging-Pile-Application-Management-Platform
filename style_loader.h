#pragma once

#include <QString>
#include <QStringList>

// 读取一组 Qt 资源中的 QSS，并按给定顺序合并。
QString loadStyleResources(const QStringList &resourcePaths);
