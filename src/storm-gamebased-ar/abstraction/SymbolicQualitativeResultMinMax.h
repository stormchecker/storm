#pragma once

#include "storm-gamebased-ar/abstraction/QualitativeResultMinMax.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace dd {
template<storm::dd::DdType Type>
class Bdd;
}
}  // namespace storm

namespace storm::gbar {
namespace abstraction {
template<storm::dd::DdType Type>
class SymbolicQualitativeResult;

template<storm::dd::DdType Type>
class SymbolicQualitativeResultMinMax : public QualitativeResultMinMax {
   public:
    SymbolicQualitativeResultMinMax() = default;

    virtual bool isSymbolic() const override;

    SymbolicQualitativeResult<Type> const& getProb0Min() const;
    SymbolicQualitativeResult<Type> const& getProb1Min() const;
    SymbolicQualitativeResult<Type> const& getProb0Max() const;
    SymbolicQualitativeResult<Type> const& getProb1Max() const;

    virtual SymbolicQualitativeResult<Type> const& getProb0(storm::OptimizationDirection const& dir) const = 0;
    virtual SymbolicQualitativeResult<Type> const& getProb1(storm::OptimizationDirection const& dir) const = 0;
};
}  // namespace abstraction
}  // namespace storm::gbar
