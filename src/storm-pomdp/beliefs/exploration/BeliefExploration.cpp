#include "storm-pomdp/beliefs/exploration/BeliefExploration.h"

#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm::pomdp::beliefs {
template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
BeliefExploration<BeliefMdpValueType, PomdpType, BeliefType>::BeliefExploration(PomdpType const& pomdp) : firstStateNextStateGenerator(pomdp) {
    // Intentionally left empty.
}

template class BeliefExploration<double, storm::models::sparse::Pomdp<double>, Belief<double>>;
template class BeliefExploration<double, storm::models::sparse::Pomdp<double>, Belief<storm::RationalNumber>>;
template class BeliefExploration<storm::RationalNumber, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<storm::RationalNumber>>;
template class BeliefExploration<double, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<storm::RationalNumber>>;
template class BeliefExploration<storm::RationalNumber, storm::models::sparse::Pomdp<double>, Belief<double>>;
}  // namespace storm::pomdp::beliefs
