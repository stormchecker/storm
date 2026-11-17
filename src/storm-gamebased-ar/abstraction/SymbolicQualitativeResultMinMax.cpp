#include "storm-gamebased-ar/abstraction/SymbolicQualitativeResultMinMax.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

#include "storm-gamebased-ar/abstraction/QualitativeResult.h"
#include "storm/dd/DdInstantiations.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
bool SymbolicQualitativeResultMinMax<Type>::isSymbolic() const {
    return true;
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeResultMinMax<Type>::getProb0Min() const {
    return getProb0(storm::OptimizationDirection::Minimize);
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeResultMinMax<Type>::getProb1Min() const {
    return getProb1(storm::OptimizationDirection::Minimize);
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeResultMinMax<Type>::getProb0Max() const {
    return getProb0(storm::OptimizationDirection::Maximize);
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeResultMinMax<Type>::getProb1Max() const {
    return getProb1(storm::OptimizationDirection::Maximize);
}

#define STORM_DD_INSTANTIATE(DD) template class SymbolicQualitativeResultMinMax<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace abstraction
}  // namespace storm::gbar
