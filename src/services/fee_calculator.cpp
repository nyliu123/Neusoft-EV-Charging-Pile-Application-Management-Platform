#include "services/fee_calculator.h"

#include <cmath>
#include <limits>

namespace ev {

Result<FeeBreakdown> FeeCalculator::calculate(long double kilowattHours,
                                              qint64 priceCentPerKilowattHour,
                                              int discountBasisPoints)
{
    if (!std::isfinite(kilowattHours) || kilowattHours < 0.0L
        || priceCentPerKilowattHour <= 0
        || discountBasisPoints <= 0 || discountBasisPoints > 10000) {
        return Result<FeeBreakdown>::fail(ErrorCode::InvalidInput,
                                          QStringLiteral("invalid fee input"));
    }

    const long double grossValue = kilowattHours * priceCentPerKilowattHour;
    if (grossValue > static_cast<long double>(std::numeric_limits<qint64>::max())) {
        return Result<FeeBreakdown>::fail(ErrorCode::InvalidInput,
                                          QStringLiteral("fee exceeds supported range"));
    }

    const qint64 grossCent = static_cast<qint64>(std::floor(grossValue + 0.5L));
    const long double netValue = static_cast<long double>(grossCent)
        * discountBasisPoints / 10000.0L;
    const qint64 netCent = static_cast<qint64>(std::floor(netValue + 0.5L));
    return Result<FeeBreakdown>::ok({grossCent, grossCent - netCent, netCent});
}

} // namespace ev

