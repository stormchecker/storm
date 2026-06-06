#include "storm-pomdp/transformer/BinaryPomdpTransformer.h"

#include <queue>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

template<typename ValueType>
BinaryPomdpTransformer<ValueType>::BinaryPomdpTransformer() {
    // Intentionally left empty
}

template<typename ValueType>
PomdpTransformationResult<ValueType> BinaryPomdpTransformer<ValueType>::transform(storm::models::sparse::Pomdp<ValueType> const& pomdp, bool transformSimple,
                                                                                  bool keepStateValuations) const {
    auto data = transformTransitions(pomdp, transformSimple);
    storm::storage::sparse::ModelComponents<ValueType> components;
    components.stateLabeling = transformStateLabeling(pomdp, data);
    for (auto const& rewModel : pomdp.getRewardModels()) {
        components.rewardModels.emplace(rewModel.first, transformRewardModel(pomdp, rewModel.second, data));
    }
    components.transitionMatrix = std::move(data.simpleMatrix);
    components.observabilityClasses = std::move(data.simpleObservations);
    if (keepStateValuations && pomdp.hasStateValuations()) {
        components.stateValuations = pomdp.getStateValuations().selectEntities(data.simpleStateToOriginalState);
    }
    PomdpTransformationResult<ValueType> result;
    result.transformedPomdp = std::make_shared<storm::models::sparse::Pomdp<ValueType>>(std::move(components), true);
    result.transformedStateToOriginalStateMap = data.simpleStateToOriginalState;
    return result;
}

struct BinaryPomdpTransformerRowGroup {
    BinaryPomdpTransformerRowGroup(uint64_t origState, uint64_t firstRow, uint64_t endRow, uint32_t origStateObservation)
        : origState(origState), firstRow(firstRow), endRow(endRow), origStateObservation(origStateObservation) {
        // Intentionally left empty.
    }

    uint64_t origState;
    uint64_t firstRow;
    uint64_t endRow;
    uint32_t origStateObservation;
    storm::storage::BitVector auxStateId;

    uint64_t size() const {
        return endRow - firstRow;
    }

    std::vector<BinaryPomdpTransformerRowGroup> split() const {
        assert(size() > 1);
        uint64_t midRow = firstRow + size() / 2;
        std::vector<BinaryPomdpTransformerRowGroup> res;
        res.emplace_back(origState, firstRow, midRow, origStateObservation);
        storm::storage::BitVector newAuxStateId = auxStateId;
        newAuxStateId.resize(auxStateId.size() + 1, false);
        res.back().auxStateId = newAuxStateId;
        res.emplace_back(origState, midRow, endRow, origStateObservation);
        newAuxStateId.set(auxStateId.size(), true);
        res.back().auxStateId = newAuxStateId;
        return res;
    }
};

struct BinaryPomdpTransformerRowGroupCompare {
    bool operator()(BinaryPomdpTransformerRowGroup const& lhs, BinaryPomdpTransformerRowGroup const& rhs) const {
        if (lhs.origStateObservation == rhs.origStateObservation) {
            return lhs.auxStateId < rhs.auxStateId;
        } else {
            return lhs.origStateObservation < rhs.origStateObservation;
        }
    }
};

template<typename ValueType>
typename BinaryPomdpTransformer<ValueType>::TransformationData BinaryPomdpTransformer<ValueType>::transformTransitions(
    storm::models::sparse::Pomdp<ValueType> const& pomdp, bool transformSimple) const {
    auto const& matrix = pomdp.getTransitionMatrix();

    // Initialize a FIFO Queue that stores the start and the end of each row group
    std::queue<BinaryPomdpTransformerRowGroup> queue;
    for (uint64_t state = 0; state < matrix.getRowGroupCount(); ++state) {
        queue.emplace(state, matrix.getRowGroupIndices()[state], matrix.getRowGroupIndices()[state + 1], pomdp.getObservation(state));
    }

    std::vector<uint32_t> newObservations;
    std::map<BinaryPomdpTransformerRowGroup, uint32_t, BinaryPomdpTransformerRowGroupCompare> observationMap;
    storm::storage::SparseMatrixBuilder<ValueType> builder(0, 0, 0, true, true);
    uint64_t currRow = 0;
    std::vector<uint64_t> origRowToSimpleRowMap(pomdp.getNumberOfChoices(), std::numeric_limits<uint64_t>::max());
    uint64_t currAuxState = queue.size();
    std::vector<uint64_t> origStates;

    while (!queue.empty()) {
        auto group = std::move(queue.front());
        queue.pop();

        // Get the observation
        uint64_t newObservation = observationMap.insert(std::make_pair(group, observationMap.size())).first->second;
        newObservations.push_back(newObservation);

        // Add matrix entries
        builder.newRowGroup(currRow);
        if (group.size() == 1) {
            // Insert the row directly
            for (auto const& entry : matrix.getRow(group.firstRow)) {
                builder.addNextValue(currRow, entry.getColumn(), entry.getValue());
            }
            origRowToSimpleRowMap[group.firstRow] = currRow;
            ++currRow;
        } else if (group.size() > 1) {
            // Split the row group into two equally large parts
            for (auto& splittedGroup : group.split()) {
                // Check whether we can insert the row now or whether an auxiliary state is needed
                if (splittedGroup.size() == 1 && (!transformSimple || matrix.getRow(splittedGroup.firstRow).getNumberOfEntries() < 2)) {
                    for (auto const& entry : matrix.getRow(splittedGroup.firstRow)) {
                        builder.addNextValue(currRow, entry.getColumn(), entry.getValue());
                    }
                    origRowToSimpleRowMap[splittedGroup.firstRow] = currRow;
                    ++currRow;
                } else {
                    queue.push(std::move(splittedGroup));
                    builder.addNextValue(currRow, currAuxState, storm::utility::one<ValueType>());
                    ++currAuxState;
                    ++currRow;
                }
            }
        }
        // Nothing to be done if group has size zero
        origStates.push_back(group.origState);
    }

    TransformationData result;
    result.simpleMatrix = builder.build(currRow, currAuxState, currAuxState);
    result.simpleObservations = std::move(newObservations);
    result.originalToSimpleChoiceMap = std::move(origRowToSimpleRowMap);
    result.simpleStateToOriginalState = std::move(origStates);
    return result;
}

template<typename ValueType>
storm::models::sparse::StateLabeling BinaryPomdpTransformer<ValueType>::transformStateLabeling(storm::models::sparse::Pomdp<ValueType> const& pomdp,
                                                                                               TransformationData const& data) const {
    storm::models::sparse::StateLabeling labeling(data.simpleMatrix.getRowGroupCount());
    for (auto const& labelName : pomdp.getStateLabeling().getLabels()) {
        storm::storage::BitVector newStates = pomdp.getStateLabeling().getStates(labelName);
        newStates.resize(data.simpleMatrix.getRowGroupCount(), false);
        if (labelName != "init") {
            for (uint64_t newState = pomdp.getNumberOfStates(); newState < data.simpleMatrix.getRowGroupCount(); ++newState) {
                newStates.set(newState, newStates[data.simpleStateToOriginalState[newState]]);
            }
        }
        labeling.addLabel(labelName, std::move(newStates));
    }
    return labeling;
}

template<typename ValueType>
storm::models::sparse::StandardRewardModel<ValueType> BinaryPomdpTransformer<ValueType>::transformRewardModel(
    storm::models::sparse::Pomdp<ValueType> const& pomdp, storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel,
    TransformationData const& data) const {
    std::optional<std::vector<ValueType>> stateRewards, actionRewards;
    if (rewardModel.hasStateRewards()) {
        stateRewards = rewardModel.getStateRewardVector();
        stateRewards.value().resize(data.simpleMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
    }
    if (rewardModel.hasStateActionRewards()) {
        actionRewards = std::vector<ValueType>(data.simpleMatrix.getRowCount(), storm::utility::zero<ValueType>());
        for (uint64_t pomdpChoice = 0; pomdpChoice < pomdp.getNumberOfChoices(); ++pomdpChoice) {
            STORM_LOG_ASSERT(data.originalToSimpleChoiceMap[pomdpChoice] < data.simpleMatrix.getRowCount(), "Invalid entry in map for choice " << pomdpChoice);
            actionRewards.value()[data.originalToSimpleChoiceMap[pomdpChoice]] = rewardModel.getStateActionReward(pomdpChoice);
        }
    }
    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException, "Transition rewards are currently not supported.");
    return storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards), std::move(actionRewards));
}

template class BinaryPomdpTransformer<storm::RationalNumber>;

template class BinaryPomdpTransformer<double>;
template class BinaryPomdpTransformer<storm::RationalFunction>;
}  // namespace transformer
}  // namespace storm