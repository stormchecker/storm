#include "QualitativeResultMinMax.h"

#include "storm-gamebased-ar/abstraction/SymbolicQualitativeResultMinMax.h"

namespace storm::gbar {
namespace abstraction {

bool QualitativeResultMinMax::isSymbolic() const {
    return false;
}

bool QualitativeResultMinMax::isExplicit() const {
    return false;
}

template<storm::dd::DdType Type>
SymbolicQualitativeResultMinMax<Type> const& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax() const {
    return static_cast<SymbolicQualitativeResultMinMax<Type> const&>(*this);
}

template<storm::dd::DdType Type>
SymbolicQualitativeResultMinMax<Type>& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax() {
    return static_cast<SymbolicQualitativeResultMinMax<Type>&>(*this);
}

template SymbolicQualitativeResultMinMax<storm::dd::DdType::CUDD> const& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax() const;
template SymbolicQualitativeResultMinMax<storm::dd::DdType::CUDD>& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax();
template SymbolicQualitativeResultMinMax<storm::dd::DdType::Sylvan> const& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax() const;
template SymbolicQualitativeResultMinMax<storm::dd::DdType::Sylvan>& QualitativeResultMinMax::asSymbolicQualitativeResultMinMax();

}  // namespace abstraction
}  // namespace storm::gbar
