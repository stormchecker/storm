#pragma once

#include <cstdint>
#include <limits>

namespace storm::modelchecker {
using RankingType = uint64_t;
static RankingType const InfRank = std::numeric_limits<RankingType>::max();
}  // namespace storm::modelchecker