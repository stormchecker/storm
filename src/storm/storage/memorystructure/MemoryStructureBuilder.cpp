#include "storm/storage/memorystructure/MemoryStructureBuilder.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename ValueType, typename RewardModelType>
MemoryStructureBuilder<ValueType, RewardModelType>::MemoryStructureBuilder(uint_fast64_t numberOfMemoryStates,
                                                                           storm::models::sparse::Model<ValueType, RewardModelType> const& model,
                                                                           bool onlyInitialStatesRelevant)
    : model(model),
      transitions(numberOfMemoryStates, std::vector<boost::optional<storm::storage::BitVector>>(numberOfMemoryStates)),
      stateLabeling(numberOfMemoryStates),
      initialMemoryStates(onlyInitialStatesRelevant ? model.getInitialStates().getNumberOfSetBits() : model.getNumberOfStates(), 0),
      onlyInitialStatesRelevant(onlyInitialStatesRelevant) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
MemoryStructureBuilder<ValueType, RewardModelType>::MemoryStructureBuilder(MemoryStructure const& memoryStructure,
                                                                           storm::models::sparse::Model<ValueType, RewardModelType> const& model)
    : model(model),
      transitions(memoryStructure.getTransitionMatrix()),
      stateLabeling(memoryStructure.getStateLabeling()),
      initialMemoryStates(memoryStructure.getInitialMemoryStates()),
      onlyInitialStatesRelevant(memoryStructure.isOnlyInitialStatesRelevantSet()) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
void MemoryStructureBuilder<ValueType, RewardModelType>::setInitialMemoryState(uint_fast64_t initialModelState, uint_fast64_t initialMemoryState) {
    STORM_LOG_THROW(!onlyInitialStatesRelevant || model.getInitialStates().get(initialModelState), storm::exceptions::InvalidOperationException,
                    "Invalid index of initial model state: " << initialMemoryState << ". This is not an initial state of the model.");
    STORM_LOG_THROW(
        initialMemoryState < transitions.size(), storm::exceptions::InvalidOperationException,
        "Invalid index of initial memory state: " << initialMemoryState << ". There are only " << transitions.size() << " states in this memory structure.");

    auto initMemStateIt = initialMemoryStates.begin();

    if (onlyInitialStatesRelevant) {
        for (auto initState : model.getInitialStates()) {
            if (initState == initialModelState) {
                *initMemStateIt = initialMemoryState;
                break;
            }
            ++initMemStateIt;
        }
    } else {
        // Consider non-initial model states
        for (uint_fast64_t state = 0; state < model.getNumberOfStates(); ++state) {
            if (state == initialModelState) {
                *initMemStateIt = initialMemoryState;
                break;
            }
            ++initMemStateIt;
        }
    }

    assert(initMemStateIt != initialMemoryStates.end());
}

template<typename ValueType, typename RewardModelType>
void MemoryStructureBuilder<ValueType, RewardModelType>::setTransition(uint_fast64_t const& startState, uint_fast64_t const& goalState,
                                                                       storm::storage::BitVector const& modelStates,
                                                                       boost::optional<storm::storage::BitVector> const& modelChoices) {
    auto const& modelTransitions = model.getTransitionMatrix();

    STORM_LOG_THROW(startState < transitions.size(), storm::exceptions::InvalidOperationException,
                    "Invalid index of start state: " << startState << ". There are only " << transitions.size() << " states in this memory structure.");
    STORM_LOG_THROW(goalState < transitions.size(), storm::exceptions::InvalidOperationException,
                    "Invalid index of goal state: " << startState << ". There are only " << transitions.size() << " states in this memory structure.");
    STORM_LOG_THROW(modelStates.size() == modelTransitions.getRowGroupCount(), storm::exceptions::InvalidOperationException,
                    "The modelStates have invalid size.");
    STORM_LOG_THROW(!modelChoices || modelChoices->size() == modelTransitions.getRowGroupCount(), storm::exceptions::InvalidOperationException,
                    "The modelChoices have invalid size.");

    // translate the two bitvectors to a single BitVector that indicates the corresponding model transitions.

    storm::storage::BitVector transitionVector(modelTransitions.getEntryCount(), false);
    if (modelChoices) {
        for (auto choice : modelChoices.get()) {
            for (auto entryIt = modelTransitions.getRow(choice).begin(); entryIt < modelTransitions.getRow(choice).end(); ++entryIt) {
                if (modelStates.get(entryIt->getColumn())) {
                    transitionVector.set(entryIt - modelTransitions.begin());
                }
            }
        }
    } else {
        for (uint_fast64_t choice = 0; choice < modelTransitions.getRowCount(); ++choice) {
            for (auto entryIt = modelTransitions.getRow(choice).begin(); entryIt < modelTransitions.getRow(choice).end(); ++entryIt) {
                if (modelStates.get(entryIt->getColumn())) {
                    transitionVector.set(entryIt - modelTransitions.begin());
                }
            }
        }
    }

    // Do not insert the transition if it is never taken.
    if (transitionVector.empty()) {
        transitions[startState][goalState] = boost::none;
    } else {
        transitions[startState][goalState] = std::move(transitionVector);
    }
}

template<typename ValueType, typename RewardModelType>
void MemoryStructureBuilder<ValueType, RewardModelType>::setLabel(uint_fast64_t const& state, std::string const& label) {
    STORM_LOG_THROW(state < transitions.size(), storm::exceptions::InvalidOperationException,
                    "Can not add label to state with index " << state << ". There are only " << transitions.size() << " states in this memory structure.");
    if (!stateLabeling.containsLabel(label)) {
        stateLabeling.addLabel(label);
    }
    stateLabeling.addLabelToState(label, state);
}

template<typename ValueType, typename RewardModelType>
MemoryStructure MemoryStructureBuilder<ValueType, RewardModelType>::build() {
    return MemoryStructure(std::move(transitions), std::move(stateLabeling), std::move(initialMemoryStates), onlyInitialStatesRelevant);
}

template<typename ValueType, typename RewardModelType>
MemoryStructure MemoryStructureBuilder<ValueType, RewardModelType>::buildTrivialMemoryStructure(
    storm::models::sparse::Model<ValueType, RewardModelType> const& model) {
    MemoryStructureBuilder<ValueType, RewardModelType> memoryBuilder(1, model);
    memoryBuilder.setTransition(0, 0, storm::storage::BitVector(model.getNumberOfStates(), true));
    return memoryBuilder.build();
}

template class MemoryStructureBuilder<double>;
template class MemoryStructureBuilder<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class MemoryStructureBuilder<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class MemoryStructureBuilder<storm::RationalNumber>;
template class MemoryStructureBuilder<storm::RationalFunction>;
template class MemoryStructureBuilder<storm::Interval>;
template class MemoryStructureBuilder<storm::RationalInterval>;

}  // namespace storage
}  // namespace storm
