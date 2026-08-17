#include "storm/storage/memorystructure/MemoryStructure.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/logic/Formulas.h"
#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

MemoryStructure::MemoryStructure(TransitionMatrix const& transitionMatrix, storm::models::sparse::StateLabeling const& memoryStateLabeling,
                                 std::vector<uint_fast64_t> const& initialMemoryStates, bool onlyInitialStatesRelevant)
    : transitions(transitionMatrix),
      stateLabeling(memoryStateLabeling),
      initialMemoryStates(initialMemoryStates),
      onlyInitialStatesRelevant(onlyInitialStatesRelevant) {
    // intentionally left empty
}

MemoryStructure::MemoryStructure(TransitionMatrix&& transitionMatrix, storm::models::sparse::StateLabeling&& memoryStateLabeling,
                                 std::vector<uint_fast64_t>&& initialMemoryStates, bool onlyInitialStatesRelevant)
    : transitions(std::move(transitionMatrix)),
      stateLabeling(std::move(memoryStateLabeling)),
      initialMemoryStates(std::move(initialMemoryStates)),
      onlyInitialStatesRelevant(onlyInitialStatesRelevant) {
    // intentionally left empty
}

bool MemoryStructure::isOnlyInitialStatesRelevantSet() const {
    return onlyInitialStatesRelevant;
}

MemoryStructure::TransitionMatrix const& MemoryStructure::getTransitionMatrix() const {
    return transitions;
}

storm::models::sparse::StateLabeling const& MemoryStructure::getStateLabeling() const {
    return stateLabeling;
}

std::vector<uint_fast64_t> const& MemoryStructure::getInitialMemoryStates() const {
    return initialMemoryStates;
}

uint_fast64_t MemoryStructure::getNumberOfStates() const {
    return transitions.size();
}

uint_fast64_t MemoryStructure::getSuccessorMemoryState(uint_fast64_t const& currentMemoryState, uint_fast64_t const& modelTransitionIndex) const {
    for (uint_fast64_t successorMemState = 0; successorMemState < getNumberOfStates(); ++successorMemState) {
        boost::optional<storm::storage::BitVector> const& matrixEntry = transitions[currentMemoryState][successorMemState];
        if ((matrixEntry) && matrixEntry.get().get(modelTransitionIndex)) {
            return successorMemState;
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "The successor memorystate for the given transition could not be found.");
    return getNumberOfStates();
}

MemoryStructure MemoryStructure::product(MemoryStructure const& rhs) const {
    uint_fast64_t lhsNumStates = this->getTransitionMatrix().size();
    uint_fast64_t rhsNumStates = rhs.getTransitionMatrix().size();
    uint_fast64_t resNumStates = lhsNumStates * rhsNumStates;

    // Transition matrix
    TransitionMatrix resultTransitions(resNumStates, std::vector<boost::optional<storm::storage::BitVector>>(resNumStates));
    uint_fast64_t resState = 0;
    for (uint_fast64_t lhsState = 0; lhsState < lhsNumStates; ++lhsState) {
        for (uint_fast64_t rhsState = 0; rhsState < rhsNumStates; ++rhsState) {
            STORM_LOG_ASSERT(resState == (lhsState * rhsNumStates) + rhsState, "Memory product state index mismatch.");
            auto& resStateTransitions = resultTransitions[resState];
            for (uint_fast64_t lhsTransitionTarget = 0; lhsTransitionTarget < lhsNumStates; ++lhsTransitionTarget) {
                auto& lhsTransition = this->getTransitionMatrix()[lhsState][lhsTransitionTarget];
                if (lhsTransition) {
                    for (uint_fast64_t rhsTransitionTarget = 0; rhsTransitionTarget < rhsNumStates; ++rhsTransitionTarget) {
                        auto& rhsTransition = rhs.getTransitionMatrix()[rhsState][rhsTransitionTarget];
                        if (rhsTransition) {
                            uint_fast64_t resTransitionTarget = (lhsTransitionTarget * rhsNumStates) + rhsTransitionTarget;
                            resStateTransitions[resTransitionTarget] = rhsTransition.get() & lhsTransition.get();
                            // If it is not possible to take the considered transition w.r.t. the considered model, we can delete it.
                            if (resStateTransitions[resTransitionTarget]->empty()) {
                                resStateTransitions[resTransitionTarget] = boost::none;
                            }
                        }
                    }
                }
            }
            ++resState;
        }
    }

    // State Labels
    storm::models::sparse::StateLabeling resultLabeling(resNumStates);
    for (std::string lhsLabel : this->getStateLabeling().getLabels()) {
        storm::storage::BitVector const& lhsLabeledStates = this->getStateLabeling().getStates(lhsLabel);
        storm::storage::BitVector resLabeledStates(resNumStates, false);
        for (auto lhsState : lhsLabeledStates) {
            for (uint_fast64_t rhsState = 0; rhsState < rhsNumStates; ++rhsState) {
                resState = (lhsState * rhsNumStates) + rhsState;
                resLabeledStates.set(resState, true);
            }
        }
        resultLabeling.addLabel(lhsLabel, std::move(resLabeledStates));
    }
    for (std::string rhsLabel : rhs.getStateLabeling().getLabels()) {
        STORM_LOG_THROW(
            !resultLabeling.containsLabel(rhsLabel), storm::exceptions::InvalidOperationException,
            "Failed to build the product of two memory structures: State labelings are not disjoint as both structures contain the label " << rhsLabel << ".");
        storm::storage::BitVector const& rhsLabeledStates = rhs.getStateLabeling().getStates(rhsLabel);
        storm::storage::BitVector resLabeledStates(resNumStates, false);
        for (auto rhsState : rhsLabeledStates) {
            for (uint_fast64_t lhsState = 0; lhsState < lhsNumStates; ++lhsState) {
                resState = (lhsState * rhsNumStates) + rhsState;
                resLabeledStates.set(resState, true);
            }
        }
        resultLabeling.addLabel(rhsLabel, std::move(resLabeledStates));
    }

    // Initial States
    std::vector<uint_fast64_t> resultInitialMemoryStates;
    STORM_LOG_THROW(this->getInitialMemoryStates().size() == rhs.getInitialMemoryStates().size(), storm::exceptions::InvalidOperationException,
                    "Tried to build the product of two memory structures that consider a different number of initial model states.");
    resultInitialMemoryStates.reserve(this->getInitialMemoryStates().size());
    auto lhsStateIt = this->getInitialMemoryStates().begin();
    auto rhsStateIt = rhs.getInitialMemoryStates().begin();
    for (; lhsStateIt != this->getInitialMemoryStates().end(); ++lhsStateIt, ++rhsStateIt) {
        resultInitialMemoryStates.push_back(*lhsStateIt * rhsNumStates + *rhsStateIt);
    }

    return MemoryStructure(std::move(resultTransitions), std::move(resultLabeling), std::move(resultInitialMemoryStates));
}

template<typename ValueType, typename RewardModelType>
SparseModelMemoryProduct<ValueType, RewardModelType> MemoryStructure::product(
    storm::models::sparse::Model<ValueType, RewardModelType> const& sparseModel) const {
    return SparseModelMemoryProduct<ValueType, RewardModelType>(sparseModel, *this);
}

std::string MemoryStructure::toString() const {
    std::stringstream stream;

    stream << "Memory Structure with " << getNumberOfStates() << " states: \n";

    for (uint_fast64_t state = 0; state < getNumberOfStates(); ++state) {
        stream << "State " << state << ": Labels = {";
        bool firstLabel = true;
        for (auto const& label : getStateLabeling().getLabelsOfState(state)) {
            if (!firstLabel) {
                stream << ", ";
            }
            firstLabel = false;
            stream << label;
        }
        stream << "}, Transitions: \n";
        for (uint_fast64_t transitionTarget = 0; transitionTarget < getNumberOfStates(); ++transitionTarget) {
            stream << "\t From " << state << " to " << transitionTarget << ": \t";
            auto const& transition = getTransitionMatrix()[state][transitionTarget];
            if (transition) {
                stream << *transition;
            } else {
                stream << "false";
            }
            stream << '\n';
        }
    }

    return stream.str();
}

template SparseModelMemoryProduct<double> MemoryStructure::product(storm::models::sparse::Model<double> const& sparseModel) const;
template SparseModelMemoryProduct<double, storm::models::sparse::StandardRewardModel<storm::Interval>> MemoryStructure::product(
    storm::models::sparse::Model<double, storm::models::sparse::StandardRewardModel<storm::Interval>> const& sparseModel) const;
template SparseModelMemoryProduct<storm::RationalNumber> MemoryStructure::product(storm::models::sparse::Model<storm::RationalNumber> const& sparseModel) const;
template SparseModelMemoryProduct<storm::RationalFunction> MemoryStructure::product(
    storm::models::sparse::Model<storm::RationalFunction> const& sparseModel) const;
}  // namespace storage
}  // namespace storm
