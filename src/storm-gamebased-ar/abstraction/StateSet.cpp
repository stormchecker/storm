#include "StateSet.h"

#include "storm-gamebased-ar/abstraction/SymbolicStateSet.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

bool StateSet::isSymbolic() const {
    return false;
}

template<storm::dd::DdType Type>
SymbolicStateSet<Type> const& StateSet::asSymbolicStateSet() const {
    return static_cast<SymbolicStateSet<Type> const&>(*this);
}

template<storm::dd::DdType Type>
SymbolicStateSet<Type>& StateSet::asSymbolicStateSet() {
    return static_cast<SymbolicStateSet<Type>&>(*this);
}

template SymbolicStateSet<storm::dd::DdType::CUDD> const& StateSet::asSymbolicStateSet() const;
template SymbolicStateSet<storm::dd::DdType::CUDD>& StateSet::asSymbolicStateSet();

template SymbolicStateSet<storm::dd::DdType::Sylvan> const& StateSet::asSymbolicStateSet() const;
template SymbolicStateSet<storm::dd::DdType::Sylvan>& StateSet::asSymbolicStateSet();

}  // namespace abstraction
}  // namespace storm::gbar
