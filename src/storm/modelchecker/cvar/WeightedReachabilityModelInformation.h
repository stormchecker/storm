#pragma once

#include <string>
#include <vector>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
template<typename ValueType>
struct WeightedReachabilityModelInformation {
    std::string rewardModelName;
    storm::storage::BitVector originalTargetStates;
    storm::storage::BitVector effectiveTargetStates;
    storm::storage::BitVector badMecStates;
    std::vector<ValueType> terminalRewards;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

template<typename ValueType>
void validateTargetStatesAreAbsorbing(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& targetStates) {
    for (auto targetState : targetStates) {
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[targetState], endRow = transitionMatrix.getRowGroupIndices()[targetState + 1]; row < endRow;
             ++row) {
            for (auto const& entry : transitionMatrix.getRow(row)) {
                STORM_LOG_THROW(entry.getColumn() == targetState, storm::exceptions::InvalidPropertyException,
                                "CVaR queries currently require all original target states to be absorbing.");
            }
        }
    }
}

template<typename ValueType>
storm::storage::BitVector computeBadMecStates(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                              storm::storage::BitVector const& initialStates,
                                              storm::storage::BitVector const& targetStates) {
    storm::storage::BitVector allStates(transitionMatrix.getRowGroupCount(), true);
    storm::storage::BitVector noStates(transitionMatrix.getRowGroupCount(), false);
    auto reachableStates = storm::utility::graph::getReachableStates(transitionMatrix, initialStates, allStates, noStates);
    auto backwardTransitions = transitionMatrix.transpose(true);
    auto statesThatCanReachTarget = storm::utility::graph::performProbGreater0(backwardTransitions, allStates, targetStates);
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(transitionMatrix, backwardTransitions, reachableStates);

    storm::storage::BitVector badMecStates(transitionMatrix.getRowGroupCount(), false);
    for (auto const& mec : mecs) {
        if (!mec.containsAnyState(statesThatCanReachTarget)) {
            for (auto const& stateChoices : mec) {
                badMecStates.set(stateChoices.first, true);
            }
        }
    }
    return badMecStates;
}

template<typename SparseMdpModelType>
WeightedReachabilityModelInformation<typename SparseMdpModelType::ValueType> extractWeightedReachabilityModelInformation(
    SparseMdpModelType const& model, CvarFormulaInformation const& formulaInformation, storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::string rewardModelName = formulaInformation.rewardModelName ? formulaInformation.rewardModelName.get() : "";
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

    auto badMecStates = computeBadMecStates(model.getTransitionMatrix(), model.getInitialStates(), targetStates);
    auto effectiveTargetStates = targetStates | badMecStates;
    auto terminalRewards = stateRewards;
    auto transitionMatrix = model.getTransitionMatrix();

    if (!badMecStates.empty()) {
        STORM_LOG_INFO(
            "CVaR preprocessing converted reachable end components that cannot reach the original target set into zero-reward absorbing terminal behaviour.");
        transitionMatrix.makeRowGroupsAbsorbing(badMecStates, true);
        for (auto state : badMecStates) {
            terminalRewards[state] = storm::utility::zero<ValueType>();
        }
    }

    return {rewardModelName,
            targetStates,
            effectiveTargetStates,
            badMecStates,
            std::move(terminalRewards),
            std::move(transitionMatrix)};
}
} // namespace cvar
} // namespace modelchecker
} // namespace storm
