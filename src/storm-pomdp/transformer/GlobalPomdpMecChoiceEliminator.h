#pragma once

#include "storm/logic/Formulas.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"

namespace storm {
namespace transformer {

template<typename ValueType>
class GlobalPomdpMecChoiceEliminator {
   public:
    /* Notice that this eliminator is only correct for memoryless, strictly randomising policies */
    GlobalPomdpMecChoiceEliminator(storm::models::sparse::Pomdp<ValueType> const& pomdp);

    // Note: this only preserves probabilities for memoryless pomdps
    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transform(storm::logic::Formula const& formula) const;

   private:
    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transformMinReward(storm::logic::EventuallyFormula const& formula) const;
    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transformMax(storm::logic::UntilFormula const& formula) const;
    storm::storage::MaximalEndComponentDecomposition<ValueType> decomposeEndComponents(storm::storage::BitVector const& subsystem,
                                                                                       storm::storage::BitVector const& ignoredStates) const;
    storm::storage::BitVector getEndComponentsWithSingleOutStates(storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs) const;
    std::vector<storm::storage::BitVector> getEndComponentChoicesPerObservation(storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs,
                                                                                storm::storage::BitVector const& consideredStates) const;

    storm::storage::BitVector checkPropositionalFormula(storm::logic::Formula const& propositionalFormula) const;

    storm::models::sparse::Pomdp<ValueType> const& pomdp;
};
}  // namespace transformer
}  // namespace storm