#pragma once

#include "storm/dd/DdType.h"

#include "storm/transformer/bisimulation/dd/Partition.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class Signature {
   public:
    Signature() = default;
    Signature(storm::dd::Add<DdType, ValueType> const& signatureAdd);

    storm::dd::Add<DdType, ValueType> const& getSignatureAdd() const;

   private:
    storm::dd::Add<DdType, ValueType> signatureAdd;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
