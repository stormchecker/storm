#pragma once

#include "storm-gamebased-ar/abstraction/QualitativeResult.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm {
namespace dd {
template<storm::dd::DdType Type>
class Bdd;
}
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type>
class SymbolicQualitativeResult : public QualitativeResult {
   public:
    virtual ~SymbolicQualitativeResult() = default;

    virtual storm::dd::Bdd<Type> const& getStates() const = 0;
};

}  // namespace abstraction
}  // namespace storm::gbar
