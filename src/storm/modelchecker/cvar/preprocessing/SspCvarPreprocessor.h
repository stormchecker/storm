#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/SolveGoal.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
namespace preprocessing {

/*!
 * Preprocesses a sparse MDP for the SSP CVaR backend.
 *
 * This normalizes the model to terminal-goal semantics and extracts the
 * choice-based costs and graph information needed by the future Pareto-front VI.
 */
template<typename SparseMdpModelType>
std::vector<typename SparseMdpModelType::ValueType> extractChoiceCostsForSsp(
    SparseMdpModelType const& model, typename SparseMdpModelType::RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::vector<ValueType> choiceCosts(model.getNumberOfChoices(), storm::utility::zero<ValueType>());
    bool hasStateRewards = rewardModel.hasStateRewards();
    bool hasStateActionRewards = rewardModel.hasStateActionRewards();

    for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
        if (targetStates[state]) {
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

template<typename ValueType>
void validatePositiveChoiceCostsOutsideGoals(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& targetStates,
                                             std::vector<ValueType> const& choiceCosts) {
    ValueType const zero = storm::utility::zero<ValueType>();
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        if (targetStates[state]) {
            continue;
        }
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
            STORM_LOG_THROW(choiceCosts[row] > zero, storm::exceptions::InvalidPropertyException,
                            "CVaR SSP preprocessing currently requires strictly positive choice costs outside goal states.");
        }
    }
}

template<typename ValueType>
uint64_t validateAndComputeMaximalChoiceCostOutsideGoals(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                         storm::storage::BitVector const& targetStates, std::vector<ValueType> const& choiceCosts) {
    uint64_t maximalChoiceCost = 0;
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        if (targetStates[state]) {
            continue;
        }
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
            STORM_LOG_THROW(storm::utility::isInteger(choiceCosts[row]), storm::exceptions::InvalidPropertyException,
                            "CVaR SSP preprocessing currently requires integer-valued choice costs.");
            maximalChoiceCost = std::max(maximalChoiceCost, storm::utility::convertNumber<uint64_t>(choiceCosts[row]));
        }
    }
    return maximalChoiceCost;
}

template<typename ValueType>
std::vector<ValueType> computeExpectedCostsToGoal(Environment const& env, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                  storm::storage::BitVector const& targetStates, std::vector<ValueType> const& choiceCosts) {
    storm::models::sparse::StandardRewardModel<ValueType> rewardModel(std::nullopt, std::vector<ValueType>(choiceCosts), std::nullopt);
    auto result = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, ValueType>::computeReachabilityRewards(
        env, storm::solver::SolveGoal<ValueType, ValueType>(storm::OptimizationDirection::Minimize), transitionMatrix, backwardTransitions, rewardModel,
        targetStates, false, false);
    return std::move(result.values);
}

template<typename SparseMdpModelType>
SspCvarPreprocessingResult<typename SparseMdpModelType::ValueType> preprocessSspCvar(Environment const& env, SparseMdpModelType const& model,
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

    auto backwardTransitions = transitionMatrix.transpose(true);
    auto reachableStates = storm::utility::graph::getReachableStates(
        transitionMatrix, model.getInitialStates(), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true),
        storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false));
    auto properStates =
        storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                             storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true), targetStates);
    STORM_LOG_THROW(reachableStates.isSubsetOf(properStates), storm::exceptions::InvalidPropertyException,
                    "CVaR SSP preprocessing currently requires a proper policy from every reachable state.");
    auto choiceCosts = extractChoiceCostsForSsp(model, rewardModel, targetStates);
    validatePositiveChoiceCostsOutsideGoals(transitionMatrix, targetStates, choiceCosts);
    uint64_t maximalChoiceCost = validateAndComputeMaximalChoiceCostOutsideGoals(transitionMatrix, targetStates, choiceCosts);
    auto expectedCostsToGoal = computeExpectedCostsToGoal(env, transitionMatrix, backwardTransitions, targetStates, choiceCosts);

    return {rewardModelName,
            *model.getInitialStates().begin(),
            targetStates,
            std::move(reachableStates),
            liftedStateRewardsToChoiceCosts,
            normalizedTargetStatesToAbsorbing,
            maximalChoiceCost,
            std::move(choiceCosts),
            std::move(expectedCostsToGoal),
            std::move(transitionMatrix)};
}

}  // namespace preprocessing
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
