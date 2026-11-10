#pragma once

#include "storm/dd/DdType.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
class SymbolicStateSet;

class StateSet {
   public:
    virtual ~StateSet() = default;

    virtual bool isSymbolic() const;

    template<storm::dd::DdType Type>
    SymbolicStateSet<Type> const& asSymbolicStateSet() const;

    template<storm::dd::DdType Type>
    SymbolicStateSet<Type>& asSymbolicStateSet();
};

}  // namespace abstraction
}  // namespace storm::gbar
