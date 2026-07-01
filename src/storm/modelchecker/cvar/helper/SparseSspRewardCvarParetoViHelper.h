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
 * Implements Pareto-front value iteration for lower-tail CVaR on an SSP-style total-reward objective.
 *
 * The helper assumes that preprocessing has normalized the model to terminal-goal semantics and validated
 * strictly positive integer per-choice rewards outside goal states. The computed frontier stores tuples
 * (p, e), where p is the probability of accumulating reward at most the current threshold and e is the
 * expected shortfall below that threshold.
 */
template<typename ValueType>
class SparseSspRewardCvarParetoViHelper {
   public:
    using ParetoFront = SspRewardParetoFront<ValueType>;
    using ParetoViOperator = SspParetoValueIterationOperator<ValueType, SspParetoFrontKind::RewardLowerTail>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    SparseSspRewardCvarParetoViHelper(CvarQueryInformation const& queryInformation,
                                      preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : queryInformation(queryInformation), preprocessingResult(preprocessingResult), paretoViOperator(preprocessingResult) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException,
                        "Scheduler extraction for CVaR SSP value iteration is not implemented yet.");

        ValueType const alpha = storm::utility::convertNumber<ValueType>(queryInformation.alpha);
        FrontierWindow frontierWindow = initializeFrontierWindow();
        auto bestCandidate = extractBestCvarCandidateFromInitialFrontier(frontierWindow[0][preprocessingResult.initialState], 0, alpha);
        if (initialFrontierHasReachedAlpha(frontierWindow[0][preprocessingResult.initialState], alpha)) {
            STORM_LOG_THROW(bestCandidate.has_value(), storm::exceptions::UnexpectedException,
                            "CVaR SSP reward value iteration reached the stopping criterion without a candidate.");
            return {bestCandidate.value(), nullptr};
        }

        FrontierLayer currentLayer(paretoViOperator.getStateCount());
        for (uint64_t rewardThreshold = 1;; ++rewardThreshold) {
            paretoViOperator.apply(rewardThreshold, frontierWindow, currentLayer);
            auto currentCandidate = extractBestCvarCandidateFromInitialFrontier(currentLayer[preprocessingResult.initialState], rewardThreshold, alpha);
            if (currentCandidate.has_value() && (!bestCandidate.has_value() || bestCandidate.value() < currentCandidate.value())) {
                bestCandidate = currentCandidate;
            }

            bool const reachedAlpha = initialFrontierHasReachedAlpha(currentLayer[preprocessingResult.initialState], alpha);
            swapFrontierLayerIntoWindow(rewardThreshold, currentLayer, frontierWindow);
            if (reachedAlpha) {
                break;
            }
        }

        STORM_LOG_THROW(bestCandidate.has_value(), storm::exceptions::UnexpectedException, "CVaR SSP reward value iteration did not find a feasible candidate.");
        return {bestCandidate.value(), nullptr};
    }

   private:
    FrontierLayer createInitialFrontierLayer(int64_t rewardThreshold) const {
        FrontierLayer baseLayer(paretoViOperator.getStateCount());
        if (rewardThreshold >= 0) {
            ParetoFront const targetFront =
                ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::convertNumber<ValueType>(rewardThreshold));
            for (auto state : paretoViOperator.getReachableTargetStates()) {
                baseLayer[state] = targetFront;
            }
        } else {
            ParetoFront const targetFront = ParetoFront::singleton(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
            for (auto state : paretoViOperator.getReachableTargetStates()) {
                baseLayer[state] = targetFront;
            }
        }
        ParetoFront const nonTargetFront = ParetoFront::singleton(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
        for (auto state : paretoViOperator.getReachableNonTargetStates()) {
            baseLayer[state] = nonTargetFront;
        }
        return baseLayer;
    }

    FrontierWindow initializeFrontierWindow() const {
        STORM_LOG_ASSERT(preprocessingResult.maximalChoiceCost > 0, "Expected a strictly positive maximal choice reward.");
        FrontierWindow frontierWindow(preprocessingResult.maximalChoiceCost, FrontierLayer(paretoViOperator.getStateCount()));
        for (int64_t rewardThreshold = 1 - static_cast<int64_t>(preprocessingResult.maximalChoiceCost); rewardThreshold <= 0; ++rewardThreshold) {
            frontierWindow[ParetoViOperator::getWindowIndex(rewardThreshold, frontierWindow.size())] = createInitialFrontierLayer(rewardThreshold);
        }
        return frontierWindow;
    }

    static void swapFrontierLayerIntoWindow(int64_t rewardThreshold, FrontierLayer& layer, FrontierWindow& frontierWindow) {
        std::swap(frontierWindow[ParetoViOperator::getWindowIndex(rewardThreshold, frontierWindow.size())], layer);
    }

    static std::optional<ValueType> extractBestCvarCandidateFromInitialFrontier(ParetoFront const& initialFrontier, uint64_t rewardThreshold,
                                                                                ValueType const& alpha) {
        if (initialFrontier.empty()) {
            return std::nullopt;
        }
        ValueType const thresholdValue = storm::utility::convertNumber<ValueType>(rewardThreshold);
        std::optional<ValueType> result;
        for (auto const& point : initialFrontier) {
            ValueType const candidate = thresholdValue - point.expectedCost / alpha;
            if (!result.has_value() || result.value() < candidate) {
                result = candidate;
            }
        }
        return result;
    }

    static bool initialFrontierHasReachedAlpha(ParetoFront const& initialFrontier, ValueType const& alpha) {
        if (initialFrontier.empty()) {
            return false;
        }
        storm::utility::ElementLess<ValueType> less;
        for (auto const& point : initialFrontier) {
            if (less(point.probability, alpha)) {
                return false;
            }
        }
        return true;
    }

    CvarQueryInformation const& queryInformation;
    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
    ParetoViOperator paretoViOperator;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
