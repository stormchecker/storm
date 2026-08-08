#pragma once

#include "storm-config.h"

#include <boost/optional.hpp>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#include <gtl/phmap.hpp>
#pragma clang diagnostic pop
#include <set>

#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/bisimulation/InternalSignatureRefiner.h"
#include "storm/storage/dd/cudd/utility.h"

namespace storm {
namespace dd {

template<storm::dd::DdType DdType>
class DdManager;

template<storm::dd::DdType DdType, typename ValueType>
class Add;

namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class Partition;

template<storm::dd::DdType DdType, typename ValueType>
class Signature;

template<typename ValueType>
class InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType> {
   public:
    InternalSignatureRefiner(storm::dd::DdManager<storm::dd::DdType::CUDD> const& manager, storm::expressions::Variable const& blockVariable,
                             std::set<storm::expressions::Variable> const& stateVariables,
                             storm::dd::Bdd<storm::dd::DdType::CUDD> const& nondeterminismVariables,
                             storm::dd::Bdd<storm::dd::DdType::CUDD> const& nonBlockVariables, InternalSignatureRefinerOptions const& options);

    Partition<storm::dd::DdType::CUDD, ValueType> refine(Partition<storm::dd::DdType::CUDD, ValueType> const& oldPartition,
                                                         Signature<storm::dd::DdType::CUDD, ValueType> const& signature);

   private:
#ifdef STORM_HAVE_CUDD
    void clearCaches();

    std::pair<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>, boost::optional<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>>> refine(
        Partition<storm::dd::DdType::CUDD, ValueType> const& oldPartition, storm::dd::Add<storm::dd::DdType::CUDD, ValueType> const& signatureAdd);

    DdNodePtr encodeBlock(uint64_t blockIndex);

    std::pair<DdNodePtr, DdNodePtr> reuseOrRelabel(DdNode* partitionNode, DdNode* nondeterminismVariablesNode, DdNode* nonBlockVariablesNode);

    std::pair<DdNodePtr, DdNodePtr> refine(DdNode* partitionNode, DdNode* signatureNode, DdNode* nondeterminismVariablesNode, DdNode* nonBlockVariablesNode);

    storm::dd::DdManager<storm::dd::DdType::CUDD> const& manager;
    ::DdManager* ddman;
    storm::expressions::Variable blockVariable;
    std::set<storm::expressions::Variable> stateVariables;

    // The cubes representing all non-block and all nondeterminism variables, respectively.
    storm::dd::Bdd<storm::dd::DdType::CUDD> nondeterminismVariables;
    storm::dd::Bdd<storm::dd::DdType::CUDD> nonBlockVariables;

    // The provided options.
    InternalSignatureRefinerOptions options;

    // The indices of the DD variables associated with the block variable.
    std::vector<uint64_t> blockDdVariableIndices;

    // A vector used for encoding block indices.
    std::vector<int> blockEncoding;

    // The current number of blocks of the new partition.
    uint64_t nextFreeBlockIndex;

    // The number of completed refinements.
    uint64_t numberOfRefinements;

    // The cache used to identify states with identical signature.
    gtl::flat_hash_map<std::pair<DdNode const*, DdNode const*>, std::pair<DdNodePtr, DdNodePtr>, CuddPointerPairHash> signatureCache;

    // The cache used to identify which old block numbers have already been reused.
    gtl::flat_hash_map<DdNode const*, ReuseWrapper> reuseBlocksCache;
#endif
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
