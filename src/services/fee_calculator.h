#pragma once

#include "common/result.h"

#include <QtGlobal>

namespace ev {

struct FeeBreakdown {
    qint64 grossCent = 0;
    qint64 discountCent = 0;
    qint64 netCent = 0;
};

class FeeCalculator final {
public:
    static Result<FeeBreakdown> calculate(long double kilowattHours,
                                          qint64 priceCentPerKilowattHour,
                                          int discountBasisPoints);
};

} // namespace ev

