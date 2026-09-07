#include "Signature.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
Signature<DdType, ValueType>::Signature(storm::dd::Add<DdType, ValueType> const& signatureAdd) : signatureAdd(signatureAdd) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& Signature<DdType, ValueType>::getSignatureAdd() const {
    return signatureAdd;
}

template class Signature<storm::dd::DdType::CUDD, double>;

template class Signature<storm::dd::DdType::Sylvan, double>;
template class Signature<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class Signature<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
