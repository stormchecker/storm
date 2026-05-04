#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/helper/SspParetoFront.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

/*!
 * Implements the Pareto-front value iteration for Conditional Value-at-Risk on an SSP-style total-cost objective.
 *
 * Supported CLI shape:
 *   storm --prism model.nm --prop 'R{"reward"}min/max=? [ F "target" ]' --cvar <alpha> --cvar:method ssp
 *
 * The helper assumes that SSP-specific preprocessing has already normalized the sparse MDP to terminal-goal
 * semantics, validated integer-valued positive choice costs outside goal states, and computed the classical
 * expected cost-to-go vector used in the base layer.
 *
 * @see https://doi.org/10.1609/aaai.v36i9.21222 for the Pareto-front value iteration on which this implementation is based.
 */
template<typename ValueType>
class SparseSspCvarParetoViHelper {
   public:
    using ParetoFront = SspParetoFront<ValueType>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    SparseSspCvarParetoViHelper(CvarQueryInformation const& queryInformation,
                                preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : queryInformation(queryInformation), preprocessingResult(preprocessingResult) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        static_cast<void>(produceScheduler);
        static_assert(sizeof(SspParetoFront<ValueType>) > 0, "Expected SSP Pareto front type to be available.");
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "CVaR for stochastic shortest path objectives is not implemented yet.");
    }

   private:
    FrontierLayer createBaseFrontierLayer() const {
        FrontierLayer baseLayer(preprocessingResult.transitionMatrix.getRowGroupCount());
        for (uint64_t state = 0; state < preprocessingResult.transitionMatrix.getRowGroupCount(); ++state) {
            if (preprocessingResult.targetStates[state]) {
                baseLayer[state] = ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
            } else if (preprocessingResult.reachableStates[state]) {
                baseLayer[state] = ParetoFront::singleton(storm::utility::zero<ValueType>(), preprocessingResult.expectedCostsToGoal[state]);
            }
        }
        return baseLayer;
    }

    FrontierWindow initializeFrontierWindow() const {
        uint64_t const windowSize = preprocessingResult.maximalChoiceCost + 1;
        FrontierWindow frontierWindow(windowSize, FrontierLayer(preprocessingResult.transitionMatrix.getRowGroupCount()));
        frontierWindow[0] = createBaseFrontierLayer();
        return frontierWindow;
    }

    ParetoFront computeActionFront(uint64_t actionRow, uint64_t costBound, FrontierWindow const& frontierWindow) const {
        uint64_t const actionCost = getChoiceCostBoundOffset(actionRow);
        if (costBound < actionCost) {
            return ParetoFront();
        }

        ParetoFront actionFront = ParetoFront::singleton(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
        uint64_t const predecessorBound = costBound - actionCost;
        FrontierLayer const& predecessorLayer = frontierWindow[predecessorBound % frontierWindow.size()];

        for (auto const& transition : preprocessingResult.transitionMatrix.getRow(actionRow)) {
            actionFront = actionFront.minkowskiSum(predecessorLayer[transition.getColumn()].scaled(transition.getValue()));
        }
        return actionFront;
    }

    FrontierLayer computeFrontierLayerForCostBound(uint64_t costBound, FrontierWindow const& frontierWindow) const {
        FrontierLayer currentLayer(preprocessingResult.transitionMatrix.getRowGroupCount());
        for (uint64_t state = 0; state < preprocessingResult.transitionMatrix.getRowGroupCount(); ++state) {
            if (!preprocessingResult.reachableStates[state]) {
                continue;
            }
            if (preprocessingResult.targetStates[state]) {
                currentLayer[state] = ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
                continue;
            }

            std::vector<ParetoFront> actionFronts;
            for (uint64_t actionRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state],
                          endRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state + 1];
                 actionRow < endRow; ++actionRow) {
                auto actionFront = computeActionFront(actionRow, costBound, frontierWindow);
                if (!actionFront.empty()) {
                    actionFronts.push_back(std::move(actionFront));
                }
            }
            currentLayer[state] = ParetoFront::convexUnion(actionFronts);
        }
        return currentLayer;
    }

    /*!
     * Evaluates one initial-state frontier for a fixed cost bound n.
     *
     * Following Algorithm 1 and the total-cost extension in the paper, a frontier for bound n induces the CVaR
     * candidate n + E / t, where E is the minimal continuation cost on the frontier at probability 1 - t.
     */
    static std::optional<ValueType> extractCvarCandidateFromInitialFrontier(SspParetoFront<ValueType> const& initialFrontier, uint64_t costBound,
                                                                            double alpha) {
        ValueType const targetProbability = storm::utility::one<ValueType>() - storm::utility::convertNumber<ValueType>(alpha);
        auto continuationCost = initialFrontier.getMinimalContinuationCostAtProbability(targetProbability);
        if (!continuationCost.has_value()) {
            return std::nullopt;
        }
        return storm::utility::convertNumber<ValueType>(costBound) +
               continuationCost.value() / storm::utility::convertNumber<ValueType>(alpha);
    }

    uint64_t getChoiceCostBoundOffset(uint64_t actionRow) const {
        return storm::utility::convertNumber<uint64_t>(preprocessingResult.choiceCosts[actionRow]);
    }

    CvarQueryInformation const& queryInformation;
    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
