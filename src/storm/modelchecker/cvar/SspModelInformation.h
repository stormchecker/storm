#pragma once

#include <string>
#include <vector>

#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarPreprocessingUtilities.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

/*!
 * Collects the normalized SSP model information needed by the future CVaR VI.
 *
 * The SSP CVaR algorithm works with per-choice costs. If the input reward model
 * only uses state rewards, we lift those rewards to equivalent state-action
 * costs by copying the state reward to each outgoing choice of the state.
 */
template<typename ValueType>
struct SspModelInformation {
    std::string rewardModelName;
    uint64_t initialState;
    storm::storage::BitVector targetStates;
    storm::storage::BitVector reachableStates;
    storm::storage::BitVector statesThatCanReachTarget;
    storm::storage::BitVector badMecStates;
    bool liftedStateRewardsToChoiceCosts;
    bool normalizedTargetStatesToAbsorbing;
    std::vector<ValueType> choiceCosts;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

template<typename SparseMdpModelType>
std::vector<typename SparseMdpModelType::ValueType> extractChoiceCostsForSsp(
    SparseMdpModelType const& model, typename SparseMdpModelType::RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::vector<ValueType> choiceCosts(model.getNumberOfChoices(), storm::utility::zero<ValueType>());
    bool hasStateRewards = rewardModel.hasStateRewards();
    bool hasStateActionRewards = rewardModel.hasStateActionRewards();

    for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
        if (targetStates[state]) {
            // Costs stop once the goal state is reached.
            continue;
        }

        ValueType stateReward = hasStateRewards ? rewardModel.getStateReward(state) : storm::utility::zero<ValueType>();
        for (uint64_t row = model.getTransitionMatrix().getRowGroupIndices()[state], endRow = model.getTransitionMatrix().getRowGroupIndices()[state + 1]; row < endRow;
             ++row) {
            choiceCosts[row] = stateReward;
            if (hasStateActionRewards) {
                choiceCosts[row] += rewardModel.getStateActionReward(row);
            }
        }
    }

    return choiceCosts;
}

template<typename SparseMdpModelType>
SspModelInformation<typename SparseMdpModelType::ValueType> extractSspModelInformation(SparseMdpModelType const& model,
                                                                                        CvarQueryInformation const& queryInformation,
                                                                                        storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::string rewardModelName = queryInformation.rewardModelName ? queryInformation.rewardModelName.get() : "";
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                    "CVaR SSP preprocessing does not support transition rewards.");

    bool liftedStateRewardsToChoiceCosts = rewardModel.hasStateRewards() && !rewardModel.hasStateActionRewards();
    if (liftedStateRewardsToChoiceCosts) {
        STORM_LOG_INFO("CVaR SSP preprocessing lifts state rewards to equivalent per-choice costs.");
    }

    auto transitionMatrix = model.getTransitionMatrix();
    bool normalizedTargetStatesToAbsorbing = false;
    for (auto targetState : targetStates) {
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[targetState], endRow = transitionMatrix.getRowGroupIndices()[targetState + 1]; row < endRow; ++row) {
            for (auto const& entry : transitionMatrix.getRow(row)) {
                if (entry.getColumn() != targetState) {
                    normalizedTargetStatesToAbsorbing = true;
                    break;
                }
            }
            if (normalizedTargetStatesToAbsorbing) {
                break;
            }
        }
        if (normalizedTargetStatesToAbsorbing) {
            break;
        }
    }
    if (normalizedTargetStatesToAbsorbing) {
        STORM_LOG_INFO("CVaR SSP preprocessing makes target states absorbing to match terminal-goal semantics.");
        transitionMatrix.makeRowGroupsAbsorbing(targetStates, true);
    }

    auto reachableStates = storm::utility::graph::getReachableStates(
        transitionMatrix, model.getInitialStates(), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true),
        storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false));
    auto statesThatCanReachTarget = computeStatesThatCanReachTarget(transitionMatrix, targetStates);
    auto badMecStates =
        computeBadMecStates(computeReachableMecs(transitionMatrix, model.getInitialStates()), statesThatCanReachTarget, transitionMatrix.getRowGroupCount());
    auto choiceCosts = extractChoiceCostsForSsp(model, rewardModel, targetStates);

    return {rewardModelName,
            *model.getInitialStates().begin(),
            targetStates,
            std::move(reachableStates),
            std::move(statesThatCanReachTarget),
            std::move(badMecStates),
            liftedStateRewardsToChoiceCosts,
            normalizedTargetStatesToAbsorbing,
            std::move(choiceCosts),
            std::move(transitionMatrix)};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
