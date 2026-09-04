#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "storm/numbers/NumberTraits.h"

namespace storm {
namespace numbers {
namespace kwek_mehlhorn {

template<typename IntegerType>
std::pair<IntegerType, IntegerType> findRational(IntegerType const& alpha, IntegerType const& beta, IntegerType const& gamma, IntegerType const& delta);

template<typename RationalType, typename ImpreciseType>
std::pair<typename NumberTraits<RationalType>::IntegerType, typename NumberTraits<RationalType>::IntegerType> truncateToRational(ImpreciseType const& value,
                                                                                                                                 uint64_t precision);

template<typename RationalType>
std::pair<typename NumberTraits<RationalType>::IntegerType, typename NumberTraits<RationalType>::IntegerType> truncateToRational(double const& value,
                                                                                                                                 uint64_t precision);

template<typename RationalType, typename ImpreciseType>
RationalType findRational(uint64_t precision, ImpreciseType const& value);

template<typename RationalType, typename ImpreciseType>
RationalType sharpen(uint64_t precision, ImpreciseType const& value);

template<typename RationalType, typename ImpreciseType>
void sharpen(uint64_t precision, std::vector<ImpreciseType> const& input, std::vector<RationalType>& output);

}  // namespace kwek_mehlhorn
}  // namespace numbers
}  // namespace storm
