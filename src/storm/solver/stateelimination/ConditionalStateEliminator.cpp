#include "storm/solver/stateelimination/ConditionalStateEliminator.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
ConditionalStateEliminator<ValueType>::ConditionalStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                                  storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                                  std::vector<ValueType>& oneStepProbabilities, storm::storage::BitVector& phiStates,
                                                                  storm::storage::BitVector& psiStates)
    : StateEliminator<ValueType>(transitionMatrix, backwardTransitions),
      oneStepProbabilities(oneStepProbabilities),
      phiStates(phiStates),
      psiStates(psiStates),
      filterLabel(StateLabel::NONE) {}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::updateValue(storm::storage::sparse::state_type const& state, ValueType const& loopProbability) {
    oneStepProbabilities[state] = storm::utility::simplify((ValueType)(loopProbability * oneStepProbabilities[state]));
}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::updatePredecessor(storm::storage::sparse::state_type const& predecessor, ValueType const& probability,
                                                              storm::storage::sparse::state_type const& state) {
    oneStepProbabilities[predecessor] = storm::utility::simplify(
        (ValueType)(oneStepProbabilities[predecessor] * storm::utility::simplify((ValueType)(probability * oneStepProbabilities[state]))));
}

template<typename ValueType>
bool ConditionalStateEliminator<ValueType>::filterPredecessor(storm::storage::sparse::state_type const& state) {
    // TODO find better solution than flag
    switch (filterLabel) {
        case StateLabel::PHI:
            return phiStates.get(state);
        case StateLabel::PSI:
            return psiStates.get(state);
        default:
            STORM_LOG_ASSERT(false, "Specific state not set.");
            return false;
    }
}

template<typename ValueType>
bool ConditionalStateEliminator<ValueType>::isFilterPredecessor() const {
    return true;
}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::setFilterPhi() {
    filterLabel = StateLabel::PHI;
}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::setFilterPsi() {
    filterLabel = StateLabel::PSI;
}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::setFilter(StateLabel const& stateLabel) {
    filterLabel = stateLabel;
}

template<typename ValueType>
void ConditionalStateEliminator<ValueType>::unsetFilter() {
    filterLabel = StateLabel::NONE;
}

template class ConditionalStateEliminator<double>;

template class ConditionalStateEliminator<storm::RationalNumber>;
template class ConditionalStateEliminator<storm::RationalFunction>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
