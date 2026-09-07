#include "SymbolicQualitativeMdpResultMinMax.h"

#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeMdpResultMinMax<Type>::getProb0(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob0Min;
    } else {
        return prob0Max;
    }
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeMdpResultMinMax<Type>::getProb1(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob1Min;
    } else {
        return prob1Max;
    }
}

template class SymbolicQualitativeMdpResultMinMax<storm::dd::DdType::CUDD>;
template class SymbolicQualitativeMdpResultMinMax<storm::dd::DdType::Sylvan>;

}  // namespace abstraction
}  // namespace storm::gbar
