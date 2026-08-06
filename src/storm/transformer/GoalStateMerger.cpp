#include "storm/transformer/GoalStateMerger.h"

#include <limits>
#include <memory>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

template<typename SparseModelType>
GoalStateMerger<SparseModelType>::GoalStateMerger(SparseModelType const& model) : originalModel(model) {
    // Intentionally left empty
}

template<typename SparseModelType>
typename GoalStateMerger<SparseModelType>::ReturnType GoalStateMerger<SparseModelType>::mergeTargetAndSinkStates(
    storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates, storm::storage::BitVector const& sinkStates,
    std::vector<std::string> const& selectedRewardModels, boost::optional<storm::storage::BitVector> const& choiceFilter) const {
    STORM_LOG_THROW(maybeStates.isDisjointFrom(targetStates) && targetStates.isDisjointFrom(sinkStates) && sinkStates.isDisjointFrom(maybeStates),
                    storm::exceptions::InvalidArgumentException,
                    "Maybestates, targetstates, and sinkstates are assumed to be disjoint when creating the submodel. However, this is not the case.");

    auto result = initialize(maybeStates, targetStates, sinkStates, choiceFilter);

    auto transitionMatrix = buildTransitionMatrix(maybeStates, result.first, result.second);
    auto labeling = buildStateLabeling(maybeStates, targetStates, sinkStates, result.first);
    auto rewardModels = buildRewardModels(maybeStates, result.first, selectedRewardModels);

    result.first.model = buildOutputModel(maybeStates, result.first, std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));

    return result.first;
}

template<typename SparseModelType>
std::pair<typename GoalStateMerger<SparseModelType>::ReturnType, uint_fast64_t> GoalStateMerger<SparseModelType>::initialize(
    storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates, storm::storage::BitVector const& sinkStates,
    boost::optional<storm::storage::BitVector> const& choiceFilter) const {
    storm::storage::SparseMatrix<typename SparseModelType::ValueType> const& origMatrix = originalModel.getTransitionMatrix();

    ReturnType result;
    result.keptChoices = storm::storage::BitVector(origMatrix.getRowCount(), false);
    result.oldToNewStateIndexMapping =
        std::vector<uint_fast64_t>(maybeStates.size(), std::numeric_limits<uint_fast64_t>::max());  // init with some invalid state

    uint_fast64_t transitionCount(0), stateCount(0);
    bool targetStateRequired = !originalModel.getInitialStates().isDisjointFrom(targetStates);
    bool sinkStateRequired = !originalModel.getInitialStates().isDisjointFrom(sinkStates);
    for (auto state : maybeStates) {
        result.oldToNewStateIndexMapping[state] = stateCount;

        auto const& endOfRowGroup = origMatrix.getRowGroupIndices()[state + 1];
        bool stateIsDeadlock = true;
        for (uint_fast64_t row = origMatrix.getRowGroupIndices()[state]; row < endOfRowGroup; ++row) {
            uint_fast64_t transitionsToMaybeStates = 0;
            bool keepThisRow(true), hasTransitionToTarget(false), hasTransitionToSink(false);
            if (!choiceFilter || choiceFilter.get().get(row)) {
                for (auto const& entry : origMatrix.getRow(row)) {
                    if (maybeStates.get(entry.getColumn())) {
                        ++transitionsToMaybeStates;
                    } else if (targetStates.get(entry.getColumn())) {
                        hasTransitionToTarget = true;
                    } else if (sinkStates.get(entry.getColumn())) {
                        hasTransitionToSink = true;
                    } else {
                        keepThisRow = false;
                        break;
                    }
                }
                if (keepThisRow) {
                    stateIsDeadlock = false;
                    result.keptChoices.set(row, true);
                    transitionCount += transitionsToMaybeStates;
                    if (hasTransitionToTarget) {
                        ++transitionCount;
                        targetStateRequired = true;
                    }
                    if (hasTransitionToSink) {
                        ++transitionCount;
                        sinkStateRequired = true;
                    }
                }
            }
        }
        STORM_LOG_THROW(!stateIsDeadlock, storm::exceptions::InvalidArgumentException, "Merging goal states leads to deadlocks!");
        ++stateCount;
    }

    // Treat the target and sink states (if these states will exist)
    if (targetStateRequired) {
        result.targetState = stateCount;
        ++stateCount;
        ++transitionCount;
        storm::utility::vector::setVectorValues(result.oldToNewStateIndexMapping, targetStates, *result.targetState);
    }

    if (sinkStateRequired) {
        result.sinkState = stateCount;
        ++stateCount;
        ++transitionCount;
        storm::utility::vector::setVectorValues(result.oldToNewStateIndexMapping, sinkStates, *result.sinkState);
    }

    return std::make_pair(std::move(result), std::move(transitionCount));
}

template<typename SparseModelType>
storm::storage::SparseMatrix<typename SparseModelType::ValueType> GoalStateMerger<SparseModelType>::buildTransitionMatrix(
    storm::storage::BitVector const& maybeStates, ReturnType const& resultData, uint_fast64_t transitionCount) const {
    storm::storage::SparseMatrix<typename SparseModelType::ValueType> const& origMatrix = originalModel.getTransitionMatrix();

    uint_fast64_t rowCount = resultData.keptChoices.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    uint_fast64_t maybeStateCount = maybeStates.getNumberOfSetBits();
    uint_fast64_t stateCount = maybeStateCount + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    storm::storage::SparseMatrixBuilder<typename SparseModelType::ValueType> builder(
        rowCount, stateCount, transitionCount, true, !origMatrix.hasTrivialRowGrouping(), origMatrix.hasTrivialRowGrouping() ? 0 : stateCount);

    uint_fast64_t currRow = 0;
    for (auto state : maybeStates) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        auto const& endOfRowGroup = origMatrix.getRowGroupIndices()[state + 1];
        for (uint_fast64_t row = resultData.keptChoices.getNextSetIndex(origMatrix.getRowGroupIndices()[state]); row < endOfRowGroup;
             row = resultData.keptChoices.getNextSetIndex(row + 1)) {
            boost::optional<typename SparseModelType::ValueType> targetValue, sinkValue;
            for (auto const& entry : origMatrix.getRow(row)) {
                uint_fast64_t const& newColumn = resultData.oldToNewStateIndexMapping[entry.getColumn()];
                if (newColumn < maybeStateCount) {
                    builder.addNextValue(currRow, newColumn, entry.getValue());
                } else if (resultData.targetState && newColumn == resultData.targetState.get()) {
                    targetValue = targetValue.is_initialized() ? *targetValue + entry.getValue() : entry.getValue();
                } else if (resultData.sinkState && newColumn == resultData.sinkState.get()) {
                    sinkValue = sinkValue.is_initialized() ? *sinkValue + entry.getValue() : entry.getValue();
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                    "There is a transition originating from a maybestate that does not lead to a maybe-, target-, or sinkstate.");
                }
            }
            if (targetValue) {
                builder.addNextValue(currRow, *resultData.targetState, storm::utility::simplify(*targetValue));
            }
            if (sinkValue) {
                builder.addNextValue(currRow, *resultData.sinkState, storm::utility::simplify(*sinkValue));
            }
            ++currRow;
        }
    }

    // Add the selfloops at target and sink
    if (resultData.targetState) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        builder.addNextValue(currRow, *resultData.targetState, storm::utility::one<typename SparseModelType::ValueType>());
        ++currRow;
    }
    if (resultData.sinkState) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        builder.addNextValue(currRow, *resultData.sinkState, storm::utility::one<typename SparseModelType::ValueType>());
        ++currRow;
    }

    return builder.build();
}

template<typename SparseModelType>
storm::models::sparse::StateLabeling GoalStateMerger<SparseModelType>::buildStateLabeling(storm::storage::BitVector const& maybeStates,
                                                                                          storm::storage::BitVector const& targetStates,
                                                                                          storm::storage::BitVector const& sinkStates,
                                                                                          ReturnType const& resultData) const {
    uint_fast64_t stateCount = maybeStates.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    storm::models::sparse::StateLabeling labeling(stateCount);

    for (auto const& label : originalModel.getStateLabeling().getLabels()) {
        storm::storage::BitVector const& oldStatesWithLabel = originalModel.getStates(label);
        storm::storage::BitVector newStatesWithLabel = oldStatesWithLabel % maybeStates;
        newStatesWithLabel.resize(stateCount, false);
        if (!oldStatesWithLabel.isDisjointFrom(targetStates) && resultData.targetState.is_initialized()) {
            newStatesWithLabel.set(*resultData.targetState, true);
        }
        if (!oldStatesWithLabel.isDisjointFrom(sinkStates) && resultData.sinkState.is_initialized()) {
            newStatesWithLabel.set(*resultData.sinkState, true);
        }
        labeling.addLabel(label, std::move(newStatesWithLabel));
    }

    return labeling;
}

template<typename SparseModelType>
std::unordered_map<std::string, typename SparseModelType::RewardModelType> GoalStateMerger<SparseModelType>::buildRewardModels(
    storm::storage::BitVector const& maybeStates, ReturnType const& resultData, std::vector<std::string> const& selectedRewardModels) const {
    typedef typename SparseModelType::RewardModelType::ValueType RewardValueType;

    uint_fast64_t maybeStateCount = maybeStates.getNumberOfSetBits();
    uint_fast64_t stateCount = maybeStateCount + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    uint_fast64_t choiceCount = resultData.keptChoices.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);

    std::unordered_map<std::string, typename SparseModelType::RewardModelType> rewardModels;
    for (auto rewardModelName : selectedRewardModels) {
        auto origRewardModel = originalModel.getRewardModel(rewardModelName);

        std::optional<std::vector<RewardValueType>> stateRewards;
        if (origRewardModel.hasStateRewards()) {
            stateRewards = storm::utility::vector::filterVector(origRewardModel.getStateRewardVector(), maybeStates);
            stateRewards->resize(stateCount, storm::utility::zero<RewardValueType>());
        }

        std::optional<std::vector<RewardValueType>> stateActionRewards;
        if (origRewardModel.hasStateActionRewards()) {
            stateActionRewards = storm::utility::vector::filterVector(origRewardModel.getStateActionRewardVector(), resultData.keptChoices);
            stateActionRewards->resize(choiceCount, storm::utility::zero<RewardValueType>());
        }

        std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards;
        if (origRewardModel.hasTransitionRewards()) {
            storm::storage::SparseMatrixBuilder<RewardValueType> builder(choiceCount, stateCount, 0, true);
            for (auto row : resultData.keptChoices) {
                boost::optional<typename SparseModelType::ValueType> targetValue, sinkValue;
                for (auto const& entry : origRewardModel.getTransitionRewardMatrix().getRow(row)) {
                    uint_fast64_t const& newColumn = resultData.oldToNewStateIndexMapping[entry.getColumn()];
                    if (newColumn < maybeStateCount) {
                        builder.addNextValue(row, newColumn, entry.getValue());
                    } else if (resultData.targetState && newColumn == resultData.targetState.get()) {
                        targetValue = targetValue.is_initialized() ? *targetValue + entry.getValue() : entry.getValue();
                    } else if (resultData.sinkState && newColumn == resultData.sinkState.get()) {
                        sinkValue = sinkValue.is_initialized() ? *sinkValue + entry.getValue() : entry.getValue();
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                        "There is a transition reward originating from a maybestate that does not lead to a maybe-, target-, or sinkstate.");
                    }
                }
                if (targetValue) {
                    builder.addNextValue(row, *resultData.targetState, storm::utility::simplify(*targetValue));
                }
                if (sinkValue) {
                    builder.addNextValue(row, *resultData.sinkState, storm::utility::simplify(*sinkValue));
                }
            }
            transitionRewards = builder.build();
        }

        rewardModels.insert(std::make_pair(
            rewardModelName, typename SparseModelType::RewardModelType(std::move(stateRewards), std::move(stateActionRewards), std::move(transitionRewards))));
    }
    return rewardModels;
}

template<>
std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> GoalStateMerger<storm::models::sparse::MarkovAutomaton<double>>::buildOutputModel(
    storm::storage::BitVector const& maybeStates, ReturnType const& resultData, storm::storage::SparseMatrix<double>&& transitionMatrix,
    storm::models::sparse::StateLabeling&& labeling,
    std::unordered_map<std::string, typename storm::models::sparse::MarkovAutomaton<double>::RewardModelType>&& rewardModels) const {
    storm::storage::sparse::ModelComponents<double> modelComponents(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));
    uint_fast64_t stateCount = maybeStates.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);

    modelComponents.markovianStates = originalModel.getMarkovianStates() % maybeStates;
    modelComponents.markovianStates->resize(stateCount, true);

    modelComponents.exitRates = storm::utility::vector::filterVector(originalModel.getExitRates(), maybeStates);
    modelComponents.exitRates->resize(stateCount, storm::utility::one<double>());

    return std::make_shared<storm::models::sparse::MarkovAutomaton<double>>(std::move(modelComponents));
}

template<>
std::shared_ptr<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>
GoalStateMerger<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::buildOutputModel(
    storm::storage::BitVector const& maybeStates, ReturnType const& resultData, storm::storage::SparseMatrix<storm::RationalNumber>&& transitionMatrix,
    storm::models::sparse::StateLabeling&& labeling,
    std::unordered_map<std::string, typename storm::models::sparse::MarkovAutomaton<storm::RationalNumber>::RewardModelType>&& rewardModels) const {
    storm::storage::sparse::ModelComponents<storm::RationalNumber> modelComponents(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));
    uint_fast64_t stateCount = maybeStates.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);

    modelComponents.markovianStates = originalModel.getMarkovianStates() % maybeStates;
    modelComponents.markovianStates->resize(stateCount, true);

    modelComponents.exitRates = storm::utility::vector::filterVector(originalModel.getExitRates(), maybeStates);
    modelComponents.exitRates->resize(stateCount, storm::utility::one<storm::RationalNumber>());

    return std::make_shared<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>(std::move(modelComponents));
}

template<typename SparseModelType>
std::shared_ptr<SparseModelType> GoalStateMerger<SparseModelType>::buildOutputModel(
    storm::storage::BitVector const&, GoalStateMerger::ReturnType const&, storm::storage::SparseMatrix<typename SparseModelType::ValueType>&& transitionMatrix,
    storm::models::sparse::StateLabeling&& labeling, std::unordered_map<std::string, typename SparseModelType::RewardModelType>&& rewardModels) const {
    return std::make_shared<SparseModelType>(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));
}

template class GoalStateMerger<storm::models::sparse::Dtmc<double>>;
template class GoalStateMerger<storm::models::sparse::Mdp<double>>;
template class GoalStateMerger<storm::models::sparse::MarkovAutomaton<double>>;
template class GoalStateMerger<storm::models::sparse::Dtmc<storm::RationalNumber>>;
template class GoalStateMerger<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class GoalStateMerger<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
template class GoalStateMerger<storm::models::sparse::Dtmc<storm::RationalFunction>>;
template class GoalStateMerger<storm::models::sparse::Mdp<storm::RationalFunction>>;

}  // namespace transformer
}  // namespace storm
