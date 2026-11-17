#include "storm/transformer/bisimulation/dd/Signature.h"
#include "storm/dd/DdInstantiations.h"

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

#define STORM_DD_INSTANTIATE(DD, ValueType) template class Signature<storm::dd::DdType::DD, ValueType>;
STORM_DD_FOR_EACH_STANDARD(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
