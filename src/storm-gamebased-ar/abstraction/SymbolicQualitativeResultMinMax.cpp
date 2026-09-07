#include "SymbolicQualitativeResultMinMax.h"

#include "storm-gamebased-ar/abstraction/QualitativeResult.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

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

template class SymbolicQualitativeResultMinMax<storm::dd::DdType::CUDD>;
template class SymbolicQualitativeResultMinMax<storm::dd::DdType::Sylvan>;

}  // namespace abstraction
}  // namespace storm::gbar
