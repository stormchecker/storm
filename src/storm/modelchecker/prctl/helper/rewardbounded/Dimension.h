#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <memory>
#include <string>

#include "storm/logic/Formula.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

enum class DimensionBoundType {
    Unbounded,          // i.e., >=0 or <=B where B approaches infinity
    UpperBound,         // i.e., <=B where B is either a constant or a variable
    LowerBound,         // i.e., >B, where B is either a constant or a variable
    LowerBoundInfinity  // i.e., >B, where B approaches infinity
};

template<typename ValueType>
struct Dimension {
    /// The formula describing this dimension
    std::shared_ptr<storm::logic::Formula const> formula;

    /// The index of the associated objective
    uint64_t objectiveIndex;

    /// A label that indicates the states where this dimension is still relevant (i.e., it is yet unknown whether the corresponding objective holds)
    boost::optional<std::string> memoryLabel;

    /// The type of the bound on this dimension.
    DimensionBoundType boundType;

    /// Multiplying an epoch value with this factor yields the reward/cost in the original domain.
    ValueType scalingFactor;

    /// The dimensions that are not satisfiable whenever the bound of this dimension is violated
    storm::storage::BitVector dependentDimensions;

    /// The maximal epoch value that needs to be considered for this dimension
    boost::optional<uint64_t> maxValue;

    /// Whether we minimize/maximize the objective for this dimension
    boost::optional<storm::solver::OptimizationDirection> optimizationDirection;
};
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
