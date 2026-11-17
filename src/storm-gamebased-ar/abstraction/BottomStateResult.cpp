#include "storm-gamebased-ar/abstraction/BottomStateResult.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
BottomStateResult<DdType>::BottomStateResult(storm::dd::Bdd<DdType> const& states, storm::dd::Bdd<DdType> const& transitions)
    : states(states), transitions(transitions) {
    // Intentionally left empty.
}

#define STORM_DD_INSTANTIATE(DD) template struct BottomStateResult<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE
}  // namespace abstraction
}  // namespace storm::gbar
