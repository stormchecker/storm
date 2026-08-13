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
#include "storm/modelchecker/cvar/helper/SspParetoValueIterationOperator.h"
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
    using ParetoViOperator = SspParetoValueIterationOperator<ValueType>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    SparseSspCvarParetoViHelper(CvarQueryInformation const& queryInformation, preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : queryInformation(queryInformation), preprocessingResult(preprocessingResult), paretoViOperator(preprocessingResult) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException,
                        "Scheduler extraction for CVaR SSP value iteration is not implemented yet.");

        FrontierWindow frontierWindow = initializeFrontierWindow();
        std::optional<ValueType> bestCandidate =
            extractCvarCandidateFromInitialFrontier(frontierWindow[0][preprocessingResult.initialState], 0, queryInformation.alpha);
        FrontierLayer currentLayer(paretoViOperator.getStateCount());

        for (uint64_t costBound = 1; !bestCandidate.has_value() || storm::utility::convertNumber<ValueType>(costBound) <= bestCandidate.value(); ++costBound) {
            paretoViOperator.apply(costBound, frontierWindow, currentLayer);
            auto currentCandidate = extractCvarCandidateFromInitialFrontier(currentLayer[preprocessingResult.initialState], costBound, queryInformation.alpha);
            if (currentCandidate.has_value() && (!bestCandidate.has_value() || currentCandidate.value() < bestCandidate.value())) {
                bestCandidate = currentCandidate;
            }
            swapFrontierLayerIntoWindow(costBound, currentLayer, frontierWindow);
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
        FrontierLayer baseLayer(paretoViOperator.getStateCount());
        ValueType const boundValue = storm::utility::convertNumber<ValueType>(costBound);
        if (costBound >= 0) {
            ParetoFront const targetFront = ParetoViOperator::createTargetFrontier();
            for (auto state : paretoViOperator.getReachableTargetStates()) {
                baseLayer[state] = targetFront;
            }
        } else {
            for (auto state : paretoViOperator.getReachableTargetStates()) {
                baseLayer[state] = ParetoFront::singleton(storm::utility::zero<ValueType>(), preprocessingResult.expectedCostsToGoal[state] - boundValue);
            }
        }
        for (auto state : paretoViOperator.getReachableNonTargetStates()) {
            baseLayer[state] = ParetoFront::singleton(storm::utility::zero<ValueType>(), preprocessingResult.expectedCostsToGoal[state] - boundValue);
        }
        return baseLayer;
    }

    FrontierWindow initializeFrontierWindow() const {
        STORM_LOG_ASSERT(preprocessingResult.maximalChoiceCost > 0, "Expected a strictly positive maximal choice cost.");
        FrontierWindow frontierWindow(preprocessingResult.maximalChoiceCost, FrontierLayer(paretoViOperator.getStateCount()));
        for (int64_t costBound = 1 - static_cast<int64_t>(preprocessingResult.maximalChoiceCost); costBound <= 0; ++costBound) {
            frontierWindow[ParetoViOperator::getWindowIndex(costBound, frontierWindow.size())] = createInitialFrontierLayer(costBound);
        }
        return frontierWindow;
    }

    static void swapFrontierLayerIntoWindow(int64_t costBound, FrontierLayer& layer, FrontierWindow& frontierWindow) {
        std::swap(frontierWindow[ParetoViOperator::getWindowIndex(costBound, frontierWindow.size())], layer);
    }

    /*!
     * Evaluates one initial-state frontier for a fixed cost bound n.
     *
     * Following Algorithm 1 and the total-cost extension in the paper, a frontier for bound n induces the CVaR
     * candidate n + E / t, where E is the minimal continuation cost on the frontier at probability 1 - t.
     */
    static std::optional<ValueType> extractCvarCandidateFromInitialFrontier(SspParetoFront<ValueType> const& initialFrontier, uint64_t costBound,
                                                                            storm::RationalNumber const& alpha) {
        ValueType const targetProbability = storm::utility::one<ValueType>() - storm::utility::convertNumber<ValueType>(alpha);
        auto continuationCost = initialFrontier.getMinimalContinuationCostAtProbability(targetProbability);
        if (!continuationCost.has_value()) {
            return std::nullopt;
        }
        return storm::utility::convertNumber<ValueType>(costBound) + continuationCost.value() / storm::utility::convertNumber<ValueType>(alpha);
    }

    CvarQueryInformation const& queryInformation;
    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
    ParetoViOperator paretoViOperator;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
