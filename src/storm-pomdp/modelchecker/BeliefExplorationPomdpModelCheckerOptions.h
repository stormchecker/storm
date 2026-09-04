#pragma once

#include "storm-pomdp/builder/BeliefMdpExplorer.h"
#include "storm/numbers/NumberTraits.h"
#include "storm/numbers/constants.h"

namespace storm {
namespace builder {
template<typename PomdpType, typename BeliefValueType>
class BeliefMdpExplorer;
}
namespace pomdp {
namespace modelchecker {
template<typename ValueType>
struct BeliefExplorationPomdpModelCheckerOptions {
    BeliefExplorationPomdpModelCheckerOptions(bool discretize, bool unfold) : discretize(discretize), unfold(unfold) {
        // Intentionally left empty
    }

    // TODO documentation?
    bool discretize;
    bool unfold;

    bool useClipping = false;
    bool interactiveUnfolding = false;
    bool refine = false;
    bool cutZeroGap = false;
    bool useStateEliminationCutoff = false;
    uint64_t refineStepLimit = 0;
    ValueType refinePrecision = storm::numbers::convert<ValueType>(1e-4);
    uint64_t explorationTimeLimit = 0;

    // Control parameters for the refinement heuristic
    // Discretization Resolution
    uint64_t resolutionInit = 2;
    ValueType resolutionFactor = storm::numbers::convert<ValueType, uint64_t>(2);
    // The maximal number of newly expanded MDP states in a refinement step
    uint64_t sizeThresholdInit = 0;
    ValueType sizeThresholdFactor = storm::numbers::convert<ValueType, uint64_t>(4);
    // Controls how large the gap between known lower- and upper bounds at a belief state needs to be in order to explore
    ValueType gapThresholdInit = storm::numbers::convert<ValueType>(0.1);
    ValueType gapThresholdFactor = storm::numbers::convert<ValueType>(0.25);
    // Controls whether "almost optimal" choices will be considered optimal
    ValueType optimalChoiceValueThresholdInit = storm::numbers::convert<ValueType>(1e-3);
    ValueType optimalChoiceValueThresholdFactor = storm::numbers::one<ValueType>();
    // Controls which observations are refined.
    ValueType obsThresholdInit = storm::numbers::convert<ValueType>(0.1);
    ValueType obsThresholdIncrementFactor = storm::numbers::convert<ValueType>(0.1);

    uint64_t clippingGridRes = 2;

    bool skipHeuristicSchedulers = false;

    ValueType numericPrecision = storm::numbers::NumberTraits<ValueType>::IsExact
                                     ? storm::numbers::zero<ValueType>()
                                     : storm::numbers::convert<ValueType>(1e-9);  /// Used to decide whether two beliefs are equal
    bool dynamicTriangulation = true;  // Sets whether the triangulation is done in a dynamic way (yielding more precise triangulations)

    storm::builder::ExplorationHeuristic explorationHeuristic = storm::builder::ExplorationHeuristic::BreadthFirst;
};
}  // namespace modelchecker
}  // namespace pomdp
}  // namespace storm
