#include "storm-gamebased-ar/abstraction/SymbolicQualitativeMdpResult.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
SymbolicQualitativeMdpResult<Type>::SymbolicQualitativeMdpResult(storm::dd::Bdd<Type> const& states) : states(states) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicQualitativeMdpResult<Type>::getStates() const {
    return states;
}

#define STORM_DD_INSTANTIATE(DD) template class SymbolicQualitativeMdpResult<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace abstraction
}  // namespace storm::gbar
