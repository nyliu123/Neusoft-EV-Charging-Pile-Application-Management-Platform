#pragma once
#include "common/result.h"
#include <QSqlDatabase>
namespace ev { Result<int> migrateMembership(QSqlDatabase &database); }
