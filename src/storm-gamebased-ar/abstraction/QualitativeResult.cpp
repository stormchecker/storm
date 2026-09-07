#include "QualitativeResult.h"

#include "storm-gamebased-ar/abstraction/ExplicitQualitativeResult.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeResult.h"

namespace storm::gbar {
namespace abstraction {

bool QualitativeResult::isSymbolic() const {
    return false;
}

bool QualitativeResult::isExplicit() const {
    return false;
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type>& QualitativeResult::asSymbolicQualitativeResult() {
    return static_cast<SymbolicQualitativeResult<Type>&>(*this);
}

template<storm::dd::DdType Type>
SymbolicQualitativeResult<Type> const& QualitativeResult::asSymbolicQualitativeResult() const {
    return static_cast<SymbolicQualitativeResult<Type> const&>(*this);
}

ExplicitQualitativeResult& QualitativeResult::asExplicitQualitativeResult() {
    return static_cast<ExplicitQualitativeResult&>(*this);
}

ExplicitQualitativeResult const& QualitativeResult::asExplicitQualitativeResult() const {
    return static_cast<ExplicitQualitativeResult const&>(*this);
}

}  // namespace abstraction
}  // namespace storm::gbar
