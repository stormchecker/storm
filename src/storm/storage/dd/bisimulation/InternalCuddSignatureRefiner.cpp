#include "storm/storage/dd/bisimulation/InternalCuddSignatureRefiner.h"

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/Signature.h"

namespace storm {
namespace dd {
namespace bisimulation {

#ifdef STORM_HAVE_CUDD
template<typename ValueType>
InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::InternalSignatureRefiner(storm::dd::DdManager<storm::dd::DdType::CUDD> const& manager,
                                                                                       storm::expressions::Variable const& blockVariable,
                                                                                       std::set<storm::expressions::Variable> const& stateVariables,
                                                                                       storm::dd::Bdd<storm::dd::DdType::CUDD> const& nondeterminismVariables,
                                                                                       storm::dd::Bdd<storm::dd::DdType::CUDD> const& nonBlockVariables,
                                                                                       InternalSignatureRefinerOptions const& options)
    : manager(manager),
      ddman(manager.getInternalDdManager().getCuddManager().getManager()),
      blockVariable(blockVariable),
      stateVariables(stateVariables),
      nondeterminismVariables(nondeterminismVariables),
      nonBlockVariables(nonBlockVariables),
      options(options),
      nextFreeBlockIndex(0),
      numberOfRefinements(0),
      signatureCache(),
      reuseBlocksCache() {
    // Initialize precomputed data.
    auto const& ddMetaVariable = manager.getMetaVariable(blockVariable);
    blockDdVariableIndices = ddMetaVariable.getIndices();

    // Create initialized block encoding where all variables are "don't care".
    blockEncoding = std::vector<int>(static_cast<uint64_t>(manager.getInternalDdManager().getCuddManager().ReadSize()), static_cast<int>(2));
}

template<typename ValueType>
Partition<storm::dd::DdType::CUDD, ValueType> InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::refine(
    Partition<storm::dd::DdType::CUDD, ValueType> const& oldPartition, Signature<storm::dd::DdType::CUDD, ValueType> const& signature) {
    std::pair<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>, boost::optional<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>>> newPartitionDds =
        refine(oldPartition, signature.getSignatureAdd());
    ++numberOfRefinements;
    return oldPartition.replacePartition(newPartitionDds.first, nextFreeBlockIndex, nextFreeBlockIndex, newPartitionDds.second);
}

template<typename ValueType>
void InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::clearCaches() {
    signatureCache.clear();
    reuseBlocksCache.clear();
}

template<typename ValueType>
std::pair<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>, boost::optional<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>>>
InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::refine(Partition<storm::dd::DdType::CUDD, ValueType> const& oldPartition,
                                                                     storm::dd::Add<storm::dd::DdType::CUDD, ValueType> const& signatureAdd) {
    STORM_LOG_ASSERT(oldPartition.storedAsAdd(), "Expecting partition to be stored as ADD for CUDD.");

    nextFreeBlockIndex = options.reuseBlockNumbers ? oldPartition.getNextFreeBlockIndex() : 0;

    // Perform the actual recursive refinement step.
    std::pair<DdNodePtr, DdNodePtr> result =
        refine(oldPartition.asAdd().getInternalAdd().getCuddDdNode(), signatureAdd.getInternalAdd().getCuddDdNode(),
               nondeterminismVariables.getInternalBdd().getCuddDdNode(), nonBlockVariables.getInternalBdd().getCuddDdNode());

    // Construct resulting ADD from the obtained node and the meta information.
    storm::dd::InternalAdd<storm::dd::DdType::CUDD, ValueType> internalNewPartitionAdd(
        &manager.getInternalDdManager(), cudd::ADD(manager.getInternalDdManager().getCuddManager(), result.first));
    storm::dd::Add<storm::dd::DdType::CUDD, ValueType> newPartitionAdd(oldPartition.asAdd().getDdManager(), internalNewPartitionAdd,
                                                                       oldPartition.asAdd().getContainedMetaVariables());

    boost::optional<storm::dd::Add<storm::dd::DdType::CUDD, ValueType>> optionalChangedAdd;
    if (result.second) {
        storm::dd::InternalAdd<storm::dd::DdType::CUDD, ValueType> internalChangedAdd(
            &manager.getInternalDdManager(), cudd::ADD(manager.getInternalDdManager().getCuddManager(), result.second));
        storm::dd::Add<storm::dd::DdType::CUDD, ValueType> changedAdd(oldPartition.asAdd().getDdManager(), internalChangedAdd, stateVariables);
        optionalChangedAdd = changedAdd;
    }

    clearCaches();
    return std::make_pair(newPartitionAdd, optionalChangedAdd);
}

template<typename ValueType>
DdNodePtr InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::encodeBlock(uint64_t blockIndex) {
    for (auto const& blockDdVariableIndex : blockDdVariableIndices) {
        blockEncoding[blockDdVariableIndex] = blockIndex & 1 ? 1 : 0;
        blockIndex >>= 1;
    }
    DdNodePtr bddEncoding = Cudd_CubeArrayToBdd(ddman, blockEncoding.data());
    Cudd_Ref(bddEncoding);
    DdNodePtr result = Cudd_BddToAdd(ddman, bddEncoding);
    Cudd_Ref(result);
    Cudd_RecursiveDeref(ddman, bddEncoding);
    Cudd_Deref(result);
    return result;
}

template<typename ValueType>
std::pair<DdNodePtr, DdNodePtr> InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::reuseOrRelabel(DdNode* partitionNode,
                                                                                                             DdNode* nondeterminismVariablesNode,
                                                                                                             DdNode* nonBlockVariablesNode) {
    // If we arrived at the constant zero node, then this was an illegal state encoding.
    if (partitionNode == Cudd_ReadZero(ddman)) {
        if (options.createChangedStates) {
            return std::make_pair(partitionNode, Cudd_ReadZero(ddman));
        } else {
            return std::make_pair(partitionNode, nullptr);
        }
    }

    // Check the cache whether we have seen the same node before.
    auto nodePair = std::make_pair(nullptr, partitionNode);

    auto it = signatureCache.find(nodePair);
    if (it != signatureCache.end()) {
        // If so, we return the corresponding result.
        return it->second;
    }

    // If there are no more non-block variables, we hit the signature.
    if (Cudd_IsConstant(nonBlockVariablesNode)) {
        if (options.reuseBlockNumbers) {
            // If this is the first time (in this traversal) that we encounter this signature, we check
            // whether we can assign the old block number to it.
            auto& reuseEntry = reuseBlocksCache[partitionNode];
            if (!reuseEntry.isReused()) {
                reuseEntry.setReused();
                std::pair<DdNodePtr, DdNodePtr> result = std::make_pair(partitionNode, options.createChangedStates ? Cudd_ReadZero(ddman) : nullptr);
                signatureCache[nodePair] = result;
                return result;
            }
        }

        std::pair<DdNodePtr, DdNodePtr> result = std::make_pair(encodeBlock(nextFreeBlockIndex++), options.createChangedStates ? Cudd_ReadOne(ddman) : nullptr);
        signatureCache[nodePair] = result;
        return result;
    } else {
        // If there are more variables that belong to the non-block part of the encoding, we need to recursively descend.

        bool skipped = true;
        DdNode* partitionThen;
        DdNode* partitionElse;
        short offset;
        bool isNondeterminismVariable;
        while (skipped && !Cudd_IsConstant(nonBlockVariablesNode)) {
            // Remember an offset that indicates whether the top variable is a nondeterminism variable or not.
            offset = options.shiftStateVariables ? 1 : 0;
            isNondeterminismVariable = false;
            if (!Cudd_IsConstant(nondeterminismVariablesNode) && Cudd_NodeReadIndex(nondeterminismVariablesNode) == Cudd_NodeReadIndex(nonBlockVariablesNode)) {
                offset = 0;
                isNondeterminismVariable = true;
            }

            if (Cudd_NodeReadIndex(partitionNode) - offset == Cudd_NodeReadIndex(nonBlockVariablesNode)) {
                partitionThen = Cudd_T(partitionNode);
                partitionElse = Cudd_E(partitionNode);
                skipped = false;
            } else {
                partitionThen = partitionElse = partitionNode;
            }

            // If we skipped the next variable, we fast-forward.
            if (skipped) {
                // If the current variable is a nondeterminism variable, we need to advance both variable sets otherwise just the non-block variables.
                nonBlockVariablesNode = Cudd_T(nonBlockVariablesNode);
                if (isNondeterminismVariable) {
                    nondeterminismVariablesNode = Cudd_T(nondeterminismVariablesNode);
                }
            }
        }

        // If there are no more non-block variables remaining, make a recursive call to enter the base case.
        if (Cudd_IsConstant(nonBlockVariablesNode)) {
            return reuseOrRelabel(partitionNode, nondeterminismVariablesNode, nonBlockVariablesNode);
        }

        std::pair<DdNodePtr, DdNodePtr> combinedThenResult = reuseOrRelabel(
            partitionThen, isNondeterminismVariable ? Cudd_T(nondeterminismVariablesNode) : nondeterminismVariablesNode, Cudd_T(nonBlockVariablesNode));
        DdNodePtr thenResult = combinedThenResult.first;
        DdNodePtr changedThenResult = combinedThenResult.second;
        Cudd_Ref(thenResult);
        if (options.createChangedStates) {
            Cudd_Ref(changedThenResult);
        } else {
            STORM_LOG_ASSERT(!changedThenResult, "Expected not changed state DD.");
        }
        std::pair<DdNodePtr, DdNodePtr> combinedElseResult = reuseOrRelabel(
            partitionElse, isNondeterminismVariable ? Cudd_T(nondeterminismVariablesNode) : nondeterminismVariablesNode, Cudd_T(nonBlockVariablesNode));
        DdNodePtr elseResult = combinedElseResult.first;
        DdNodePtr changedElseResult = combinedElseResult.second;
        Cudd_Ref(elseResult);
        if (options.createChangedStates) {
            Cudd_Ref(changedElseResult);
        } else {
            STORM_LOG_ASSERT(!changedThenResult, "Expected not changed state DD.");
        }

        DdNodePtr result;
        if (thenResult == elseResult) {
            Cudd_Deref(thenResult);
            Cudd_Deref(elseResult);
            result = thenResult;
        } else {
            // Get the node to connect the subresults.
            bool complemented = Cudd_IsComplement(thenResult);
            result = cuddUniqueInter(ddman, Cudd_NodeReadIndex(nonBlockVariablesNode) + offset, Cudd_Regular(thenResult),
                                     complemented ? Cudd_Not(elseResult) : elseResult);
            if (complemented) {
                result = Cudd_Not(result);
            }
            Cudd_Deref(thenResult);
            Cudd_Deref(elseResult);
        }

        DdNodePtr changedResult = nullptr;
        if (options.createChangedStates) {
            if (changedThenResult == changedElseResult) {
                Cudd_Deref(changedThenResult);
                Cudd_Deref(changedElseResult);
                changedResult = changedThenResult;
            } else {
                // Get the node to connect the subresults.
                bool complemented = Cudd_IsComplement(changedThenResult);
                changedResult = cuddUniqueInter(ddman, Cudd_NodeReadIndex(nonBlockVariablesNode) + offset, Cudd_Regular(changedThenResult),
                                                complemented ? Cudd_Not(changedElseResult) : changedElseResult);
                if (complemented) {
                    changedResult = Cudd_Not(changedResult);
                }
                Cudd_Deref(changedThenResult);
                Cudd_Deref(changedElseResult);
            }
        }

        // Store the result in the cache.
        auto pairResult = std::make_pair(result, changedResult);
        signatureCache[nodePair] = pairResult;
        return pairResult;
    }
}

template<typename ValueType>
std::pair<DdNodePtr, DdNodePtr> InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::refine(DdNode* partitionNode, DdNode* signatureNode,
                                                                                                     DdNode* nondeterminismVariablesNode,
                                                                                                     DdNode* nonBlockVariablesNode) {
    // If we arrived at the constant zero node, then this was an illegal state encoding.
    if (partitionNode == Cudd_ReadZero(ddman)) {
        if (options.createChangedStates) {
            return std::make_pair(partitionNode, Cudd_ReadZero(ddman));
        } else {
            return std::make_pair(partitionNode, nullptr);
        }
    } else if (signatureNode == Cudd_ReadZero(ddman)) {
        std::pair<DdNodePtr, DdNodePtr> result = reuseOrRelabel(partitionNode, nondeterminismVariablesNode, nonBlockVariablesNode);
        signatureCache[std::make_pair(signatureNode, partitionNode)] = result;
        return result;
    }

    // Check the cache whether we have seen the same node before.
    auto nodePair = std::make_pair(signatureNode, partitionNode);

    auto it = signatureCache.find(nodePair);
    if (it != signatureCache.end()) {
        // If so, we return the corresponding result.
        return it->second;
    }

    // If there are no more non-block variables, we hit the signature.
    if (Cudd_IsConstant(nonBlockVariablesNode)) {
        if (options.reuseBlockNumbers) {
            // If this is the first time (in this traversal) that we encounter this signature, we check
            // whether we can assign the old block number to it.
            auto& reuseEntry = reuseBlocksCache[partitionNode];
            if (!reuseEntry.isReused()) {
                reuseEntry.setReused();
                std::pair<DdNodePtr, DdNodePtr> result = std::make_pair(partitionNode, options.createChangedStates ? Cudd_ReadZero(ddman) : nullptr);
                signatureCache[nodePair] = result;
                return result;
            }
        }

        std::pair<DdNodePtr, DdNodePtr> result = std::make_pair(encodeBlock(nextFreeBlockIndex++), options.createChangedStates ? Cudd_ReadOne(ddman) : nullptr);
        signatureCache[nodePair] = result;
        return result;
    } else {
        // If there are more variables that belong to the non-block part of the encoding, we need to recursively descend.

        bool skippedBoth = true;
        DdNode* partitionThen;
        DdNode* partitionElse;
        DdNode* signatureThen;
        DdNode* signatureElse;
        short offset;
        bool isNondeterminismVariable;
        while (skippedBoth && !Cudd_IsConstant(nonBlockVariablesNode)) {
            // Remember an offset that indicates whether the top variable is a nondeterminism variable or not.
            offset = options.shiftStateVariables ? 1 : 0;
            isNondeterminismVariable = false;
            if (!Cudd_IsConstant(nondeterminismVariablesNode) && Cudd_NodeReadIndex(nondeterminismVariablesNode) == Cudd_NodeReadIndex(nonBlockVariablesNode)) {
                offset = 0;
                isNondeterminismVariable = true;
            }

            if (Cudd_NodeReadIndex(partitionNode) - offset == Cudd_NodeReadIndex(nonBlockVariablesNode)) {
                partitionThen = Cudd_T(partitionNode);
                partitionElse = Cudd_E(partitionNode);
                skippedBoth = false;
            } else {
                partitionThen = partitionElse = partitionNode;
            }

            if (Cudd_NodeReadIndex(signatureNode) == Cudd_NodeReadIndex(nonBlockVariablesNode)) {
                signatureThen = Cudd_T(signatureNode);
                signatureElse = Cudd_E(signatureNode);
                skippedBoth = false;
            } else {
                signatureThen = signatureElse = signatureNode;
            }

            // If both (signature and partition) skipped the next variable, we fast-forward.
            if (skippedBoth) {
                // If the current variable is a nondeterminism variable, we need to advance both variable sets otherwise just the non-block variables.
                nonBlockVariablesNode = Cudd_T(nonBlockVariablesNode);
                if (isNondeterminismVariable) {
                    nondeterminismVariablesNode = Cudd_T(nondeterminismVariablesNode);
                }
            }
        }

        // If there are no more non-block variables remaining, make a recursive call to enter the base case.
        if (Cudd_IsConstant(nonBlockVariablesNode)) {
            return refine(partitionNode, signatureNode, nondeterminismVariablesNode, nonBlockVariablesNode);
        }

        std::pair<DdNodePtr, DdNodePtr> combinedThenResult =
            refine(partitionThen, signatureThen, isNondeterminismVariable ? Cudd_T(nondeterminismVariablesNode) : nondeterminismVariablesNode,
                   Cudd_T(nonBlockVariablesNode));
        DdNodePtr thenResult = combinedThenResult.first;
        DdNodePtr changedThenResult = combinedThenResult.second;
        Cudd_Ref(thenResult);
        if (options.createChangedStates) {
            Cudd_Ref(changedThenResult);
        } else {
            STORM_LOG_ASSERT(!changedThenResult, "Expected not changed state DD.");
        }
        std::pair<DdNodePtr, DdNodePtr> combinedElseResult =
            refine(partitionElse, signatureElse, isNondeterminismVariable ? Cudd_T(nondeterminismVariablesNode) : nondeterminismVariablesNode,
                   Cudd_T(nonBlockVariablesNode));
        DdNodePtr elseResult = combinedElseResult.first;
        DdNodePtr changedElseResult = combinedElseResult.second;
        Cudd_Ref(elseResult);
        if (options.createChangedStates) {
            Cudd_Ref(changedElseResult);
        } else {
            STORM_LOG_ASSERT(!changedThenResult, "Expected not changed state DD.");
        }

        DdNodePtr result;
        if (thenResult == elseResult) {
            Cudd_Deref(thenResult);
            Cudd_Deref(elseResult);
            result = thenResult;
        } else {
            // Get the node to connect the subresults.
            bool complemented = Cudd_IsComplement(thenResult);
            result = cuddUniqueInter(ddman, Cudd_NodeReadIndex(nonBlockVariablesNode) + offset, Cudd_Regular(thenResult),
                                     complemented ? Cudd_Not(elseResult) : elseResult);
            if (complemented) {
                result = Cudd_Not(result);
            }
            Cudd_Deref(thenResult);
            Cudd_Deref(elseResult);
        }

        DdNodePtr changedResult = nullptr;
        if (options.createChangedStates) {
            if (changedThenResult == changedElseResult) {
                Cudd_Deref(changedThenResult);
                Cudd_Deref(changedElseResult);
                changedResult = changedThenResult;
            } else {
                // Get the node to connect the subresults.
                bool complemented = Cudd_IsComplement(changedThenResult);
                changedResult = cuddUniqueInter(ddman, Cudd_NodeReadIndex(nonBlockVariablesNode) + offset, Cudd_Regular(changedThenResult),
                                                complemented ? Cudd_Not(changedElseResult) : changedElseResult);
                if (complemented) {
                    changedResult = Cudd_Not(changedResult);
                }
                Cudd_Deref(changedThenResult);
                Cudd_Deref(changedElseResult);
            }
        }

        // Store the result in the cache.
        auto pairResult = std::make_pair(result, changedResult);
        signatureCache[nodePair] = pairResult;
        return pairResult;
    }
}
#else
template<typename ValueType>
InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::InternalSignatureRefiner(storm::dd::DdManager<storm::dd::DdType::CUDD> const& manager,
                                                                                       storm::expressions::Variable const& blockVariable,
                                                                                       std::set<storm::expressions::Variable> const& stateVariables,
                                                                                       storm::dd::Bdd<storm::dd::DdType::CUDD> const& nondeterminismVariables,
                                                                                       storm::dd::Bdd<storm::dd::DdType::CUDD> const& nonBlockVariables,
                                                                                       InternalSignatureRefinerOptions const& options) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
Partition<storm::dd::DdType::CUDD, ValueType> InternalSignatureRefiner<storm::dd::DdType::CUDD, ValueType>::refine(
    Partition<storm::dd::DdType::CUDD, ValueType> const& oldPartition, Signature<storm::dd::DdType::CUDD, ValueType> const& signature) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}
#endif

template class InternalSignatureRefiner<storm::dd::DdType::CUDD, double>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
