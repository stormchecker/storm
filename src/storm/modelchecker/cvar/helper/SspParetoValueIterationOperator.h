#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "storm/modelchecker/cvar/helper/SspParetoFront.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

/*!
 * Applies one cost-bound layer update for SSP CVaR Pareto-front value iteration.
 *
 * This class follows the role of Storm's generic value-iteration operators, but keeps the SSP-specific
 * cost-window semantics explicit: each action row reads from the predecessor layer determined by the current
 * cost bound minus that action's integer cost.
 */
template<typename ValueType>
class SspParetoValueIterationOperator {
   public:
    using ParetoFront = SspParetoFront<ValueType>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    explicit SspParetoValueIterationOperator(preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : preprocessingResult(preprocessingResult),
          choiceCostOffsets(createChoiceCostOffsets(preprocessingResult)),
          reachableTargetStates(collectReachableStates(preprocessingResult, true)),
          reachableNonTargetStates(collectReachableStates(preprocessingResult, false)) {
    }

    void apply(uint64_t costBound, FrontierWindow const& frontierWindow, FrontierLayer& outputLayer) const {
        prepareOutputLayer(outputLayer);

        ParetoFront const targetFront = createTargetFrontier();
        for (auto state : reachableTargetStates) {
            outputLayer[state] = targetFront;
        }

        std::vector<ParetoFront> actionFrontStorage;
        ActionFrontReducer actionFrontReducer(actionFrontStorage);
        for (auto state : reachableNonTargetStates) {
            applyRowGroup(state, costBound, frontierWindow, outputLayer, actionFrontReducer);
        }
    }

    std::size_t getStateCount() const {
        return preprocessingResult.transitionMatrix.getRowGroupCount();
    }

    std::vector<uint64_t> const& getReachableTargetStates() const {
        return reachableTargetStates;
    }

    std::vector<uint64_t> const& getReachableNonTargetStates() const {
        return reachableNonTargetStates;
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

    static ParetoFront createTargetFrontier() {
        return ParetoFront::singleton(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
    }

   private:
    class ActionFrontReducer {
       public:
        explicit ActionFrontReducer(std::vector<ParetoFront>& actionFrontStorage) : actionFrontStorage(actionFrontStorage) {}

        void reset(std::size_t rowCount) {
            actionFrontStorage.clear();
            rowCountHint = rowCount;
            firstNonEmptyActionFront.clear();
            hasNonEmptyActionFront = false;
            usesActionFrontStorage = false;
        }

        void add(ParetoFront&& actionFront) {
            addActionFront(std::move(actionFront));
        }

        void reduceInto(ParetoFront& outputFront) {
            if (!hasNonEmptyActionFront) {
                outputFront.clear();
            } else if (!usesActionFrontStorage) {
                outputFront = std::move(firstNonEmptyActionFront);
            } else {
                outputFront = ParetoFront::convexUnionDestructive(actionFrontStorage);
            }
        }

       private:
        void addActionFront(ParetoFront&& actionFront) {
            if (actionFront.empty()) {
                return;
            }
            if (!hasNonEmptyActionFront) {
                firstNonEmptyActionFront = std::move(actionFront);
                hasNonEmptyActionFront = true;
                return;
            }
            if (!usesActionFrontStorage) {
                actionFrontStorage.reserve(rowCountHint);
                actionFrontStorage.push_back(std::move(firstNonEmptyActionFront));
                usesActionFrontStorage = true;
            }
            actionFrontStorage.push_back(std::move(actionFront));
        }

        std::vector<ParetoFront>& actionFrontStorage;
        ParetoFront firstNonEmptyActionFront;
        std::size_t rowCountHint{0};
        bool hasNonEmptyActionFront{false};
        bool usesActionFrontStorage{false};
    };

    void prepareOutputLayer(FrontierLayer& layer) const {
        if (layer.size() != getStateCount()) {
            layer.resize(getStateCount());
        }
        for (auto& front : layer) {
            front.clear();
        }
    }

    void applyRowGroup(uint64_t state, uint64_t costBound, FrontierWindow const& frontierWindow, FrontierLayer& outputLayer,
                       ActionFrontReducer& actionFrontReducer) const {
        uint64_t const firstActionRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state];
        uint64_t const endActionRow = preprocessingResult.transitionMatrix.getRowGroupIndices()[state + 1];
        STORM_LOG_ASSERT(firstActionRow < endActionRow, "Expected at least one action row in each reachable non-target SSP state.");

        actionFrontReducer.reset(endActionRow - firstActionRow);
        for (uint64_t actionRow = firstActionRow; actionRow < endActionRow; ++actionRow) {
            actionFrontReducer.add(computeActionFront(actionRow, costBound, frontierWindow));
        }
        actionFrontReducer.reduceInto(outputLayer[state]);
    }

    ParetoFront computeActionFront(uint64_t actionRow, uint64_t costBound, FrontierWindow const& frontierWindow) const {
        uint64_t const actionCost = choiceCostOffsets[actionRow];
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

    FrontierLayer const& getFrontierLayerForBound(int64_t costBound, FrontierWindow const& frontierWindow) const {
        return frontierWindow[getWindowIndex(costBound, frontierWindow.size())];
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

    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
    std::vector<uint64_t> choiceCostOffsets;
    std::vector<uint64_t> reachableTargetStates;
    std::vector<uint64_t> reachableNonTargetStates;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
