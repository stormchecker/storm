#pragma once

#include "storm-config.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/bisimulation/InternalSignatureRefiner.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"
#include "storm/storage/dd/sylvan/utility.h"

namespace storm {
namespace dd {
template<storm::dd::DdType DdType>
class DdManager;

namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class Partition;

template<storm::dd::DdType DdType, typename ValueType>
class Signature;

class InternalSylvanSignatureRefinerBase {
   public:
    InternalSylvanSignatureRefinerBase(storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager, storm::expressions::Variable const& blockVariable,
                                       std::set<storm::expressions::Variable> const& stateVariables,
                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables, InternalSignatureRefinerOptions const& options);

#ifdef STORM_HAVE_SYLVAN
    storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager;
    storm::expressions::Variable blockVariable;
    std::set<storm::expressions::Variable> stateVariables;

    storm::dd::Bdd<storm::dd::DdType::Sylvan> nondeterminismVariables;
    storm::dd::Bdd<storm::dd::DdType::Sylvan> nonBlockVariables;

    // The provided options.
    InternalSignatureRefinerOptions options;

    uint64_t numberOfBlockVariables;

    storm::dd::Bdd<storm::dd::DdType::Sylvan> blockCube;

    // The current number of blocks of the new partition.
    uint64_t nextFreeBlockIndex;

    // The number of completed refinements.
    uint64_t numberOfRefinements;

    // Data used by sylvan implementation.
    std::vector<BDD> signatures;
    uint64_t currentCapacity;
    std::vector<uint64_t> table;
    std::vector<uint64_t> oldTable;
    uint64_t resizeFlag;
#endif
};

template<typename ValueType>
class InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType> : public InternalSylvanSignatureRefinerBase {
   public:
    InternalSignatureRefiner(storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager, storm::expressions::Variable const& blockVariable,
                             std::set<storm::expressions::Variable> const& stateVariables,
                             storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
                             storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables, InternalSignatureRefinerOptions const& options);

    Partition<storm::dd::DdType::Sylvan, ValueType> refine(Partition<storm::dd::DdType::Sylvan, ValueType> const& oldPartition,
                                                           Signature<storm::dd::DdType::Sylvan, ValueType> const& signature);

   private:
#ifdef STORM_HAVE_SYLVAN
    void clearCaches();

    std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>>> refine(
        Partition<storm::dd::DdType::Sylvan, ValueType> const& oldPartition, storm::dd::Add<storm::dd::DdType::Sylvan, ValueType> const& signatureAdd);
#endif
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
