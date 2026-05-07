#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
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

    SparseSspCvarParetoViHelper(CvarQueryInformation const& queryInformation, preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : queryInformation(queryInformation),
          preprocessingResult(preprocessingResult),
          choiceCostOffsets(createChoiceCostOffsets(preprocessingResult)),
          reachableTargetStates(collectReachableStates(preprocessingResult, true)),
          reachableNonTargetStates(collectReachableStates(preprocessingResult, false)) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException,
                        "Scheduler extraction for CVaR SSP value iteration is not implemented yet.");

        FrontierWindow frontierWindow = initializeFrontierWindow();
        std::optional<ValueType> bestCandidate =
            extractCvarCandidateFromInitialFrontier(frontierWindow[0][preprocessingResult.initialState], 0, queryInformation.alpha);

        for (uint64_t costBound = 1; !bestCandidate.has_value() || storm::utility::convertNumber<ValueType>(costBound) <= bestCandidate.value(); ++costBound) {
            auto currentLayer = computeFrontierLayerForCostBound(costBound, frontierWindow);
            auto currentCandidate = extractCvarCandidateFromInitialFrontier(currentLayer[preprocessingResult.initialState], costBound, queryInformation.alpha);
            if (currentCandidate.has_value() && (!bestCandidate.has_value() || currentCandidate.value() < bestCandidate.value())) {
                bestCandidate = currentCandidate;
            }
            writeFrontierLayerToWindow(costBound, std::move(currentLayer), frontierWindow);
        }

        STORM_LOG_THROW(bestCandidate.has_value(), storm::exceptions::UnexpectedException, "CVaR SSP value iteration did not find a feasible candidate.");
        return {bestCandidate.value(), nullptr};
    }

   private:
    /*!
     * Creates the initial frontier layer for a given bound n.
     *
     * For n < 0 we intentionally deviate from the paper's P_n = empty convention and instead use the
     * mathematically direct excess-cost semantics induced by E[(X-n)^+]: because all costs are nonnegative,
     * Pr[X <= n] = 0 and E[(X-n)^+] = E[X] - n. Hence the Pareto set collapses to the singleton
     * (0, e*(s) - n), where e*(s) is the minimal expected cost-to-go from s.
     */
    FrontierLayer createInitialFrontierLayer(int64_t costBound) const {
        FrontierLayer baseLayer(preprocessingResult.transitionMatrix.getRowGroupCount());
        ValueType const boundValue = storm::utility::convertNumber<ValueType>(costBound);
        for (auto state : reachableTargetStates) {
            if (costBound >= 0) {
                baseLayer[state] = ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
            } else {
                baseLayer[state] = ParetoFront::singleton(storm::utility::zero<ValueType>(), preprocessingResult.expectedCostsToGoal[state] - boundValue);
            }
        }
        for (auto state : reachableNonTargetStates) {
            baseLayer[state] = ParetoFront::singleton(storm::utility::zero<ValueType>(), preprocessingResult.expectedCostsToGoal[state] - boundValue);
        }
        return baseLayer;
    }

    FrontierWindow initializeFrontierWindow() const {
        STORM_LOG_ASSERT(preprocessingResult.maximalChoiceCost > 0, "Expected a strictly positive maximal choice cost.");
        FrontierWindow frontierWindow(preprocessingResult.maximalChoiceCost, FrontierLayer(preprocessingResult.transitionMatrix.getRowGroupCount()));
        for (int64_t costBound = 1 - static_cast<int64_t>(preprocessingResult.maximalChoiceCost); costBound <= 0; ++costBound) {
            frontierWindow[getWindowIndex(costBound)] = createInitialFrontierLayer(costBound);
        }
        return frontierWindow;
    }

    static void writeFrontierLayerToWindow(int64_t costBound, FrontierLayer layer, FrontierWindow& frontierWindow) {
        frontierWindow[getWindowIndex(costBound, frontierWindow.size())] = std::move(layer);
    }

    FrontierLayer const& getFrontierLayerForBound(int64_t costBound, FrontierWindow const& frontierWindow) const {
        return frontierWindow[getWindowIndex(costBound, frontierWindow.size())];
    }

    ParetoFront computeActionFront(uint64_t actionRow, uint64_t costBound, FrontierWindow const& frontierWindow) const {
        uint64_t const actionCost = getChoiceCostBoundOffset(actionRow);
        ParetoFront actionFront;
        int64_t const predecessorBound = static_cast<int64_t>(costBound) - static_cast<int64_t>(actionCost);
        FrontierLayer const& predecessorLayer = getFrontierLayerForBound(predecessorBound, frontierWindow);

        bool initialized = false;
        for (auto const& transition : preprocessingResult.transitionMatrix.getRow(actionRow)) {
            auto const& successorFront = predecessorLayer[transition.getColumn()];
            if (initialized) {
                actionFront = actionFront.minkowskiSumScaled(successorFront, transition.getValue());
            } else {
                actionFront = successorFront.scaled(transition.getValue());
                initialized = true;
            }
        }
        return actionFront;
    }

    FrontierLayer computeFrontierLayerForCostBound(uint64_t costBound, FrontierWindow const& frontierWindow) const {
        FrontierLayer currentLayer(preprocessingResult.transitionMatrix.getRowGroupCount());
        for (auto state : reachableTargetStates) {
            currentLayer[state] = ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
        }
        for (auto state : reachableNonTargetStates) {

            uint64_t const firstActionRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state];
            uint64_t const endActionRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state + 1];
            if (firstActionRow + 1 == endActionRow) {
                currentLayer[state] = computeActionFront(firstActionRow, costBound, frontierWindow);
                continue;
            }

            ParetoFront firstNonEmptyActionFront;
            typename ParetoFront::container_type actionFrontPoints;
            bool hasNonEmptyActionFront = false;
            for (uint64_t actionRow = firstActionRow; actionRow < endActionRow; ++actionRow) {
                auto actionFront = computeActionFront(actionRow, costBound, frontierWindow);
                if (actionFront.empty()) {
                    continue;
                }
                if (!hasNonEmptyActionFront) {
                    firstNonEmptyActionFront = std::move(actionFront);
                    hasNonEmptyActionFront = true;
                    continue;
                }
                if (actionFrontPoints.empty()) {
                    actionFrontPoints.reserve(firstNonEmptyActionFront.size() + actionFront.size());
                    actionFrontPoints.insert(actionFrontPoints.end(), firstNonEmptyActionFront.begin(), firstNonEmptyActionFront.end());
                }
                actionFrontPoints.insert(actionFrontPoints.end(), actionFront.begin(), actionFront.end());
            }
            currentLayer[state] = actionFrontPoints.empty() ? std::move(firstNonEmptyActionFront) : ParetoFront(std::move(actionFrontPoints));
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
        return storm::utility::convertNumber<ValueType>(costBound) + continuationCost.value() / storm::utility::convertNumber<ValueType>(alpha);
    }

    uint64_t getChoiceCostBoundOffset(uint64_t actionRow) const {
        return choiceCostOffsets[actionRow];
    }

    static std::size_t getWindowIndex(int64_t costBound, std::size_t windowSize) {
        STORM_LOG_ASSERT(windowSize > 0, "Expected a non-empty SSP frontier window.");
        int64_t const signedWindowSize = static_cast<int64_t>(windowSize);
        int64_t index = costBound % signedWindowSize;
        if (index < 0) {
            index += signedWindowSize;
        }
        return static_cast<std::size_t>(index);
    }

    std::size_t getWindowIndex(int64_t costBound) const {
        return getWindowIndex(costBound, preprocessingResult.maximalChoiceCost);
    }

    static std::vector<uint64_t> createChoiceCostOffsets(preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult) {
        std::vector<uint64_t> result;
        result.reserve(preprocessingResult.choiceCosts.size());
        for (auto const& choiceCost : preprocessingResult.choiceCosts) {
            result.push_back(storm::utility::convertNumber<uint64_t>(choiceCost));
        }
        return result;
    }

    static std::vector<uint64_t> collectReachableStates(preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult, bool targetStates) {
        std::vector<uint64_t> result;
        for (uint64_t state = 0; state < preprocessingResult.transitionMatrix.getRowGroupCount(); ++state) {
            if (preprocessingResult.reachableStates[state] && preprocessingResult.targetStates[state] == targetStates) {
                result.push_back(state);
            }
        }
        return result;
    }

    CvarQueryInformation const& queryInformation;
    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
    std::vector<uint64_t> choiceCostOffsets;
    std::vector<uint64_t> reachableTargetStates;
    std::vector<uint64_t> reachableNonTargetStates;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
