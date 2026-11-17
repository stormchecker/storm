#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResultMinMax.h"
#include "storm/dd/DdInstantiations.h"

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

#define STORM_DD_INSTANTIATE(DD) template class SymbolicQualitativeGameResultMinMax<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace abstraction
}  // namespace storm::gbar
