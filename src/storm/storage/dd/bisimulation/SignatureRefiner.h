#pragma once

#include <memory>

#include "storm/storage/dd/DdType.h"

#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/Signature.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class InternalSignatureRefiner;

template<storm::dd::DdType DdType, typename ValueType>
class SignatureRefiner {
   public:
    SignatureRefiner(storm::dd::DdManager<DdType> const& manager, storm::expressions::Variable const& blockVariable,
                     std::set<storm::expressions::Variable> const& stateRowVariables, std::set<storm::expressions::Variable> const& stateColumnVariables,
                     bool shiftStateVariables, std::set<storm::expressions::Variable> const& nondeterminismVariables,
                     BisimulationOptions const& bisimulationOptions);

    Partition<DdType, ValueType> refine(Partition<DdType, ValueType> const& oldPartition, Signature<DdType, ValueType> const& signature);

   private:
    // The manager responsible for the DDs.
    storm::dd::DdManager<DdType> const* manager;

    // The internal refiner.
    std::shared_ptr<InternalSignatureRefiner<DdType, ValueType>> internalRefiner;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
