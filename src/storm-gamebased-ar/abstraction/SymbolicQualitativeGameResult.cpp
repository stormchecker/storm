#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResult.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
SymbolicQualitativeGameResult<Type>::SymbolicQualitativeGameResult(storm::utility::graph::SymbolicGameProb01Result<Type> const& prob01Result)
    : storm::utility::graph::SymbolicGameProb01Result<Type>(prob01Result) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicQualitativeGameResult<Type>::getStates() const {
    return this->getPlayer1States();
}

#define STORM_DD_INSTANTIATE(DD) template class SymbolicQualitativeGameResult<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE
}  // namespace abstraction
}  // namespace storm::gbar
