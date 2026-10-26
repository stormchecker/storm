#include "storm-gamebased-ar/abstraction/SymbolicQuantitativeGameResultMinMax.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type, typename ValueType>
SymbolicQuantitativeGameResultMinMax<Type, ValueType>::SymbolicQuantitativeGameResultMinMax(SymbolicQuantitativeGameResult<Type, ValueType> const& min,
                                                                                            SymbolicQuantitativeGameResult<Type, ValueType> const& max)
    : min(min), max(max) {
    // Intentionally left empty.
}

}  // namespace abstraction
}  // namespace storm::gbar
