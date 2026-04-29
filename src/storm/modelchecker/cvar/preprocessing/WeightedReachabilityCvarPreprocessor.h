#pragma once

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/cvar/CvarPreprocessingUtilities.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/preprocessing/WeightedReachabilityCvarPreprocessingResult.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
namespace preprocessing {

/*!
 * Preprocesses a sparse MDP for the weighted-reachability CVaR LP backend.
 *
 * This enforces the weighted-reachability assumptions on the reward model and
 * transition structure and returns the normalized terminal-reward instance that
 * can be consumed by the LP helper.
 */
template<typename ValueType>
storm::storage::MaximalEndComponentDecomposition<ValueType> computeTargetReachingMecs(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                      storm::storage::BitVector const& initialStates,
                                                                                      storm::storage::BitVector const& effectiveTargetStates) {
    storm::storage::BitVector allStates(transitionMatrix.getRowGroupCount(), true);
    storm::storage::BitVector nonTargetStates = ~effectiveTargetStates;
    storm::storage::BitVector noStates(allStates.size(), false);
    auto reachableStates = storm::utility::graph::getReachableStates(transitionMatrix, initialStates, allStates, noStates);
    auto subsystemStates = reachableStates & nonTargetStates;

    storm::storage::BitVector possibleEcRows(transitionMatrix.getRowCount(), false);
    for (auto state : subsystemStates) {
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
            possibleEcRows.set(row, true);
        }
    }

    auto backwardTransitions = transitionMatrix.transpose(true);
    auto statesThatCanReachTarget = storm::utility::graph::performProbGreater0(backwardTransitions, allStates, effectiveTargetStates);
    auto targetReachingStates = statesThatCanReachTarget & subsystemStates;
    return storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, targetReachingStates, possibleEcRows);
}

template<typename ValueType>
void applyTargetReachingMecCollapse(storm::storage::SparseMatrix<ValueType>& transitionMatrix, storm::storage::BitVector& effectiveTargetStates,
                                    storm::storage::BitVector& badMecStates, std::vector<ValueType>& terminalRewards, uint64_t& initialState,
                                    storm::storage::MaximalEndComponentDecomposition<ValueType> const& targetReachingMecs) {
    storm::storage::BitVector allStates(transitionMatrix.getRowGroupCount(), true);
    storm::storage::BitVector noSinkRows(transitionMatrix.getRowGroupCount(), false);
    auto eliminationResult = storm::transformer::EndComponentEliminator<ValueType>::transform(transitionMatrix, targetReachingMecs, allStates, noSinkRows);

    storm::storage::BitVector newEffectiveTargetStates(eliminationResult.matrix.getRowGroupCount(), false);
    storm::storage::BitVector newBadMecStates(eliminationResult.matrix.getRowGroupCount(), false);
    std::vector<ValueType> newTerminalRewards(eliminationResult.matrix.getRowGroupCount(), storm::utility::zero<ValueType>());
    for (auto oldTargetState : effectiveTargetStates) {
        auto newTargetState = eliminationResult.oldToNewStateMapping[oldTargetState];
        if (newTargetState < eliminationResult.matrix.getRowGroupCount()) {
            newEffectiveTargetStates.set(newTargetState, true);
            newTerminalRewards[newTargetState] = terminalRewards[oldTargetState];
        }
    }
    for (auto oldBadMecState : badMecStates) {
        auto newBadMecState = eliminationResult.oldToNewStateMapping[oldBadMecState];
        if (newBadMecState < eliminationResult.matrix.getRowGroupCount()) {
            newBadMecStates.set(newBadMecState, true);
        }
    }

    initialState = eliminationResult.oldToNewStateMapping[initialState];
    effectiveTargetStates = std::move(newEffectiveTargetStates);
    badMecStates = std::move(newBadMecStates);
    terminalRewards = std::move(newTerminalRewards);
    transitionMatrix = std::move(eliminationResult.matrix);
}

template<typename SparseMdpModelType>
WeightedReachabilityCvarPreprocessingResult<typename SparseMdpModelType::ValueType> preprocessWeightedReachabilityCvar(
    SparseMdpModelType const& model, CvarQueryInformation const& queryInformation, storm::storage::BitVector const& targetStates,
    bool produceScheduler = false) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::string rewardModelName = queryInformation.rewardModelName ? queryInformation.rewardModelName.get() : "";
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    STORM_LOG_THROW(rewardModel.hasOnlyStateRewards(), storm::exceptions::InvalidPropertyException,
                    "CVaR queries currently only support weighted reachability with state-based terminal rewards.");

    std::vector<ValueType> const& stateRewards = rewardModel.getStateRewardVector();
    validateTargetStatesAreAbsorbing(model.getTransitionMatrix(), targetStates);

    bool hasNonZeroTargetReward = false;
    for (uint64_t state = 0; state < stateRewards.size(); ++state) {
        if (targetStates[state]) {
            hasNonZeroTargetReward |= !storm::utility::isZero(stateRewards[state]);
        } else {
            STORM_LOG_THROW(storm::utility::isZero(stateRewards[state]), storm::exceptions::InvalidPropertyException,
                            "CVaR queries currently require terminal rewards, i.e. non-target states must have reward 0.");
        }
    }

    if (!hasNonZeroTargetReward) {
        STORM_LOG_WARN("All target states have terminal reward 0 in reward model '" << rewardModelName << "'.");
    }

    auto reachableMecs = computeReachableMecs(model.getTransitionMatrix(), model.getInitialStates());
    auto statesThatCanReachTarget = computeStatesThatCanReachTarget(model.getTransitionMatrix(), targetStates);
    auto badMecStates = computeBadMecStates(reachableMecs, statesThatCanReachTarget, model.getNumberOfStates());
    auto effectiveTargetStates = targetStates | badMecStates;
    auto terminalRewards = stateRewards;
    auto transitionMatrix = model.getTransitionMatrix();
    uint64_t initialState = *model.getInitialStates().begin();

    if (!badMecStates.empty()) {
        STORM_LOG_INFO(
            "CVaR preprocessing converted reachable end components that cannot reach the original target set into zero-reward absorbing terminal behaviour.");
        transitionMatrix.makeRowGroupsAbsorbing(badMecStates, true);
        for (auto state : badMecStates) {
            terminalRewards[state] = storm::utility::zero<ValueType>();
        }
    }

    uint64_t collapsedTargetReachingMecCount = 0;
    auto targetReachingMecs = computeTargetReachingMecs(transitionMatrix, model.getInitialStates(), effectiveTargetStates);
    if (!targetReachingMecs.empty()) {
        STORM_LOG_THROW(!produceScheduler, storm::exceptions::InvalidOperationException,
                        "Cannot produce a CVaR scheduler because preprocessing has to collapse target-reaching end components.");
        collapsedTargetReachingMecCount = targetReachingMecs.size();
        auto oldStateCount = transitionMatrix.getRowGroupCount();
        applyTargetReachingMecCollapse(transitionMatrix, effectiveTargetStates, badMecStates, terminalRewards, initialState, targetReachingMecs);
        STORM_PRINT_AND_LOG("CVaR preprocessing collapsed " << collapsedTargetReachingMecCount
                                                            << " target-reaching end component(s), reducing the transition matrix from " << oldStateCount
                                                            << " to " << transitionMatrix.getRowGroupCount() << " states.\n");
    }

    return {rewardModelName,
            initialState,
            targetStates,
            effectiveTargetStates,
            badMecStates,
            collapsedTargetReachingMecCount,
            std::move(terminalRewards),
            std::move(transitionMatrix)};
}

}  // namespace preprocessing
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
