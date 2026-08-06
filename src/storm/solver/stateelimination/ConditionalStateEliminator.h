#pragma once

#include "storm/solver/stateelimination/StateEliminator.h"

#include "storm/storage/BitVector.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
class ConditionalStateEliminator : public StateEliminator<ValueType> {
   public:
    enum class StateLabel { NONE, PHI, PSI };

    ConditionalStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                               storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, std::vector<ValueType>& oneStepProbabilities,
                               storm::storage::BitVector& phiStates, storm::storage::BitVector& psiStates);

    // Instantiaton of Virtual methods
    void updateValue(storm::storage::sparse::state_type const& state, ValueType const& loopProbability) override;
    void updatePredecessor(storm::storage::sparse::state_type const& predecessor, ValueType const& probability,
                           storm::storage::sparse::state_type const& state) override;
    bool filterPredecessor(storm::storage::sparse::state_type const& state) override;
    bool isFilterPredecessor() const override;

    void setFilterPhi();
    void setFilterPsi();
    void setFilter(StateLabel const& stateLabel);
    void unsetFilter();

   private:
    std::vector<ValueType>& oneStepProbabilities;
    storm::storage::BitVector& phiStates;
    storm::storage::BitVector& psiStates;
    StateLabel filterLabel;
};

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
