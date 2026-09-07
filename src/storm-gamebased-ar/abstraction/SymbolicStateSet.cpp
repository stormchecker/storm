#include "SymbolicStateSet.h"

#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
SymbolicStateSet<Type>::SymbolicStateSet(storm::dd::Bdd<Type> const& states) : states(states) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
bool SymbolicStateSet<Type>::isSymbolic() const {
    return true;
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicStateSet<Type>::getStates() const {
    return states;
}

template class SymbolicStateSet<storm::dd::DdType::CUDD>;
template class SymbolicStateSet<storm::dd::DdType::Sylvan>;

}  // namespace abstraction
}  // namespace storm::gbar
