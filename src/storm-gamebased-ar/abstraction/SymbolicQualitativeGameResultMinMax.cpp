#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResultMinMax.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeGameResultMinMax<Type>::getProb0(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob0Min;
    } else {
        return prob0Max;
    }
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& SymbolicQualitativeGameResultMinMax<Type>::getProb1(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob1Min;
    } else {
        return prob1Max;
    }
}

template class SymbolicQualitativeGameResultMinMax<storm::dd::DdType::CUDD>;
template class SymbolicQualitativeGameResultMinMax<storm::dd::DdType::Sylvan>;

}  // namespace abstraction
}  // namespace storm::gbar
