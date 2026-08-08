#include "KnownProbabilityTransformer.h"

namespace storm {
namespace pomdp {
namespace transformer {

template<typename ValueType>
KnownProbabilityTransformer<ValueType>::KnownProbabilityTransformer() {
    // Intentionally left empty
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> KnownProbabilityTransformer<ValueType>::transform(storm::models::sparse::Pomdp<ValueType> const &pomdp,
                                                                                                           storm::storage::BitVector &prob0States,
                                                                                                           storm::storage::BitVector &prob1States) {
    std::map<uint64_t, uint64_t> stateMap;
    std::map<uint32_t, uint32_t> observationMap;

    uint64_t nrNewStates = prob0States.empty() ? 1 : 2;

    storm::models::sparse::StateLabeling newLabeling(pomdp.getNumberOfStates() - prob0States.getNumberOfSetBits() - prob1States.getNumberOfSetBits() +
                                                     nrNewStates);

    std::vector<uint32_t> newObservations;

    // New state 0 represents all states with probability 1
    for (uint64_t iter : prob1States) {
        stateMap[iter] = 0;

        std::set<std::string> labelSet = pomdp.getStateLabeling().getLabelsOfState(iter);
        for (auto const &label : labelSet) {
            if (!newLabeling.containsLabel(label)) {
                newLabeling.addLabel(label);
            }
            newLabeling.addLabelToState(label, 0);
        }
    }
    // New state 1 represents all states with probability 0
    for (uint64_t iter : prob0States) {
        stateMap[iter] = 1;
        for (auto const &label : pomdp.getStateLabeling().getLabelsOfState(iter)) {
            if (!newLabeling.containsLabel(label)) {
                newLabeling.addLabel(label);
            }
            newLabeling.addLabelToState(label, 1);
        }
    }

    storm::storage::BitVector unknownStates = ~(prob1States | prob0States);
    // If there are no states with probability 0 we set the next new state id to be 1, otherwise 2
    uint64_t newId = prob0States.empty() ? 1 : 2;
    uint64_t nextObservation = prob0States.empty() ? 1 : 2;
    for (uint64_t iter : unknownStates) {
        stateMap[iter] = newId;
        if (observationMap.count(pomdp.getObservation(iter)) == 0) {
            observationMap[pomdp.getObservation(iter)] = nextObservation;
            ++nextObservation;
        }
        for (auto const &label : pomdp.getStateLabeling().getLabelsOfState(iter)) {
            if (!newLabeling.containsLabel(label)) {
                newLabeling.addLabel(label);
            }
            newLabeling.addLabelToState(label, newId);
        }
        ++newId;
    }

    uint64_t newNrOfStates = pomdp.getNumberOfStates() - (prob1States.getNumberOfSetBits() + prob0States.getNumberOfSetBits());

    uint64_t currentRow = 0;
    uint64_t currentRowGroup = 0;
    storm::storage::SparseMatrixBuilder<ValueType> smb(0, 0, 0, false, true);
    // new row for prob 1 state
    smb.newRowGroup(currentRow);
    smb.addNextValue(currentRow, 0, storm::utility::one<ValueType>());
    newObservations.push_back(0);
    ++currentRowGroup;
    ++currentRow;
    if (!prob0States.empty()) {
        smb.newRowGroup(currentRow);
        smb.addNextValue(currentRow, 1, storm::utility::one<ValueType>());
        ++currentRowGroup;
        ++currentRow;
        newObservations.push_back(1);
    }

    auto transitionMatrix = pomdp.getTransitionMatrix();

    for (uint64_t iter : unknownStates) {
        smb.newRowGroup(currentRow);
        // First collect all transitions
        // auto rowGroup = transitionMatrix.getRowGroup(iter);
        for (uint64_t row = 0; row < transitionMatrix.getRowGroupSize(iter); ++row) {
            std::map<uint64_t, ValueType> transitionsInAction;
            for (auto const &entry : transitionMatrix.getRow(iter, row)) {
                // here we use the state mapping to collect all probabilities to get to a state with prob 0/1
                transitionsInAction[stateMap[entry.getColumn()]] += entry.getValue();
            }
            for (auto const &transition : transitionsInAction) {
                smb.addNextValue(currentRow, transition.first, transition.second);
            }
            ++currentRow;
        }
        ++currentRowGroup;
        newObservations.push_back(observationMap[pomdp.getObservation(iter)]);
    }

    auto newTransitionMatrix = smb.build(currentRow, newNrOfStates, currentRowGroup);
    // STORM_PRINT(newTransitionMatrix)
    storm::storage::sparse::ModelComponents<ValueType> components(newTransitionMatrix, newLabeling);
    components.observabilityClasses = newObservations;

    return std::make_shared<storm::models::sparse::Pomdp<ValueType>>(std::move(components), true);
}

template class KnownProbabilityTransformer<double>;
template class KnownProbabilityTransformer<storm::RationalNumber>;
}  // namespace transformer
}  // namespace pomdp
}  // namespace storm