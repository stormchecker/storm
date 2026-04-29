#pragma once

#include "storm/adapters/RationalNumberForward.h"

namespace storm::utility {

/*!
 * Finds the "simplest" rational number in the given interval, where "simplest" means having the smallest denominator
 * @pre lowerBound < upperBound or (lowerBound == upperBound and lowerInclusive and upperInclusive)
 * @param lowerBound The lower bound of the interval
 * @param lowerInclusive Whether the lower bound itself is included
 * @param upperBound the upper bound of the interval
 * @param upperInclusive Whether the upper bound itself is included
 * @return the rational number in the given interval with the smallest denominator
 */
storm::RationalNumber findRational(storm::RationalNumber const& lowerBound, bool lowerInclusive, storm::RationalNumber const& upperBound, bool upperInclusive);

}  // namespace storm::utility