#pragma once

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <optional>

#include "RegionSplitEstimateKind.h"

namespace storm::modelchecker {
struct RegionSplittingStrategy {
   public:
    enum class Heuristic { EstimateBased, RoundRobin, Default };
    Heuristic heuristic{Heuristic::Default};
    uint64_t maxSplitDimensions{std::numeric_limits<uint64_t>::max()};
    std::optional<RegionSplitEstimateKind> estimateKind;

    RegionSplittingStrategy() = default;
    RegionSplittingStrategy(Heuristic heuristic, uint64_t maxSplitDimensions, std::optional<RegionSplitEstimateKind> estimateKind)
        : heuristic(heuristic), maxSplitDimensions(maxSplitDimensions), estimateKind(estimateKind) {
              // Intentionally left empty.
          };
};

std::ostream& operator<<(std::ostream& os, RegionSplittingStrategy::Heuristic const& regionCheckResult);
}  // namespace storm::modelchecker
