#include "storm/storage/dd/bisimulation/InternalSylvanSignatureRefiner.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/Signature.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

#ifdef STORM_HAVE_SYLVAN
#include "sylvan_cache.h"

// The cas macro from storm/adapters/sylvan.h and the direct __sync_fetch_and_add expand to __sync_* builtins
// -Watomic-implicit-seq-cst fires at these use sites, so suppress it here.
#pragma clang diagnostic ignored "-Watomic-implicit-seq-cst"
#endif

namespace storm {
namespace dd {
namespace bisimulation {

#ifdef STORM_HAVE_SYLVAN
static const uint64_t NO_ELEMENT_MARKER = -1ull;

InternalSylvanSignatureRefinerBase::InternalSylvanSignatureRefinerBase(storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager,
                                                                       storm::expressions::Variable const& blockVariable,
                                                                       std::set<storm::expressions::Variable> const& stateVariables,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables,
                                                                       InternalSignatureRefinerOptions const& options)
    : manager(manager),
      blockVariable(blockVariable),
      stateVariables(stateVariables),
      nondeterminismVariables(nondeterminismVariables),
      nonBlockVariables(nonBlockVariables),
      options(options),
      numberOfBlockVariables(manager.getMetaVariable(blockVariable).getNumberOfDdVariables()),
      blockCube(manager.getMetaVariable(blockVariable).getCube()),
      nextFreeBlockIndex(0),
      numberOfRefinements(0),
      currentCapacity(1ull << 20),
      resizeFlag(0) {
    table.resize(3 * currentCapacity, NO_ELEMENT_MARKER);
}

template<typename ValueType>
InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::InternalSignatureRefiner(
    storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager, storm::expressions::Variable const& blockVariable,
    std::set<storm::expressions::Variable> const& stateVariables, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables, InternalSignatureRefinerOptions const& options)
    : storm::dd::bisimulation::InternalSylvanSignatureRefinerBase(manager, blockVariable, stateVariables, nondeterminismVariables, nonBlockVariables, options) {
    // Intentionally left empty.
}

template<typename ValueType>
Partition<storm::dd::DdType::Sylvan, ValueType> InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::refine(
    Partition<storm::dd::DdType::Sylvan, ValueType> const& oldPartition, Signature<storm::dd::DdType::Sylvan, ValueType> const& signature) {
    std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>>> newPartitionDds =
        refine(oldPartition, signature.getSignatureAdd());
    ++numberOfRefinements;

    return oldPartition.replacePartition(newPartitionDds.first, nextFreeBlockIndex, nextFreeBlockIndex, newPartitionDds.second);
}

template<typename ValueType>
void InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::clearCaches() {
    for (auto& e : this->table) {
        e = NO_ELEMENT_MARKER;
    }
    for (auto& e : this->signatures) {
        e = 0ull;
    }
}

template<typename ValueType>
std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>>>
InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::refine(Partition<storm::dd::DdType::Sylvan, ValueType> const& oldPartition,
                                                                       storm::dd::Add<storm::dd::DdType::Sylvan, ValueType> const& signatureAdd) {
    STORM_LOG_ASSERT(oldPartition.storedAsBdd(), "Expecting partition to be stored as BDD for Sylvan.");

    nextFreeBlockIndex = options.reuseBlockNumbers ? oldPartition.getNextFreeBlockIndex() : 0;
    signatures.resize(nextFreeBlockIndex);

    // Perform the actual recursive refinement step.
    std::pair<BDD, BDD> result(0, 0);
    result.first =
        RUN(sylvan_refine_partition, signatureAdd.getInternalAdd().getSylvanMtbdd().GetMTBDD(), oldPartition.asBdd().getInternalBdd().getSylvanBdd().GetBDD(),
            nondeterminismVariables.getInternalBdd().getSylvanBdd().GetBDD(), nonBlockVariables.getInternalBdd().getSylvanBdd().GetBDD(), this);

    // Construct resulting BDD from the obtained node and the meta information.
    storm::dd::InternalBdd<storm::dd::DdType::Sylvan> internalNewPartitionBdd(&manager.getInternalDdManager(), sylvan::Bdd(result.first));
    storm::dd::Bdd<storm::dd::DdType::Sylvan> newPartitionBdd(oldPartition.asBdd().getDdManager(), internalNewPartitionBdd,
                                                              oldPartition.asBdd().getContainedMetaVariables());

    boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>> optionalChangedBdd;
    if (options.createChangedStates && result.second != 0) {
        storm::dd::InternalBdd<storm::dd::DdType::Sylvan> internalChangedBdd(&manager.getInternalDdManager(), sylvan::Bdd(result.second));
        storm::dd::Bdd<storm::dd::DdType::Sylvan> changedBdd(oldPartition.asBdd().getDdManager(), internalChangedBdd, stateVariables);
        optionalChangedBdd = changedBdd;
    }

    clearCaches();
    return std::make_pair(newPartitionBdd, optionalChangedBdd);
}

/* Rotating 64-bit FNV-1a hash */
static uint64_t sylvan_hash(uint64_t a, uint64_t b) {
    const uint64_t prime = 1099511628211;
    uint64_t hash = 14695981039346656037LLU;
    hash = (hash ^ (a >> 32));
    hash = (hash ^ a) * prime;
    hash = (hash ^ b) * prime;
    return hash ^ (hash >> 32);
}

/*!
 * The code below was taken from the SigrefMC implementation by Tom van Dijk, available at
 *
 *  https://github.com/utwente-fmt/sigrefmc/
 *
 * It provides a fully multi-threaded version of signature-based partition refinement and includes, amongst
 * other things, a small custom-tailored hashmap that supports multi-threaded insertion.
 *
 * The code was modified in minor places to account for necessary changes, for example to handle
 * nondeterminism variables.
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

VOID_TASK_3(sylvan_rehash, size_t, first, size_t, count, InternalSylvanSignatureRefinerBase*, refiner) {
    if (count > 128) {
        SPAWN(sylvan_rehash, first, count / 2, refiner);
        CALL(sylvan_rehash, first + count / 2, count - count / 2, refiner);
        SYNC(sylvan_rehash);
        return;
    }

    while (count--) {
        uint64_t* old_ptr = refiner->oldTable.data() + first * 3;
        uint64_t a = old_ptr[0];
        uint64_t b = old_ptr[1];
        uint64_t c = old_ptr[2];

        uint64_t hash = sylvan_hash(a, b);
        uint64_t pos = hash % refiner->currentCapacity;

        volatile uint64_t* ptr = 0;
        for (;;) {
            ptr = refiner->table.data() + pos * 3;
            if (*ptr == 0) {
                if (cas(ptr, 0, a)) {
                    ptr[1] = b;
                    ptr[2] = c;
                    break;
                }
            }
            pos++;
            if (pos >= refiner->currentCapacity)
                pos = 0;
        }

        first++;
    }
}

VOID_TASK_1(sylvan_grow_it, InternalSylvanSignatureRefinerBase*, refiner) {
    refiner->oldTable = std::move(refiner->table);

    uint64_t oldCapacity = refiner->currentCapacity;
    refiner->currentCapacity <<= 1;
    refiner->table = std::vector<uint64_t>(3 * refiner->currentCapacity, NO_ELEMENT_MARKER);

    CALL(sylvan_rehash, 0, oldCapacity, refiner);

    refiner->oldTable.clear();
}

VOID_TASK_1(sylvan_grow, InternalSylvanSignatureRefinerBase*, refiner) {
    if (cas(&refiner->resizeFlag, 0, 1)) {
        NEWFRAME(sylvan_grow_it, refiner);
        refiner->resizeFlag = 0;
    } else {
        /* wait for new frame to appear */
        while (ATOMIC_READ(lace_newframe.t) == 0) {
        }
        lace_yield(__lace_worker, __lace_dq_head);
    }
}

static uint64_t sylvan_search_or_insert(uint64_t sig, uint64_t previous_block, InternalSylvanSignatureRefinerBase* refiner) {
    uint64_t hash = sylvan_hash(sig, previous_block);
    uint64_t pos = hash % refiner->currentCapacity;

    volatile uint64_t* ptr = 0;
    uint64_t a, b, c;
    int count = 0;
    for (;;) {
        ptr = refiner->table.data() + pos * 3;
        a = *ptr;
        if (a == sig) {
            while ((b = ptr[1]) == NO_ELEMENT_MARKER) continue;
            if (b == previous_block) {
                while ((c = ptr[2]) == NO_ELEMENT_MARKER) continue;
                return c;
            }
        } else if (a == NO_ELEMENT_MARKER) {
            if (cas(ptr, NO_ELEMENT_MARKER, sig)) {
                ptr[2] = __sync_fetch_and_add(&refiner->nextFreeBlockIndex, 1);
                c = ptr[2];
                ptr[1] = previous_block;
                return c;
            } else {
                continue;
            }
        }
        pos++;
        if (pos >= refiner->currentCapacity)
            pos = 0;
        if (++count >= 128)
            return NO_ELEMENT_MARKER;
    }
}

TASK_1(uint64_t, sylvan_decode_block, BDD, block) {
    uint64_t result = 0;
    uint64_t mask = 1;
    while (block != sylvan_true) {
        BDD b_low = sylvan_low(block);
        if (b_low == sylvan_false) {
            result |= mask;
            block = sylvan_high(block);
        } else {
            block = b_low;
        }
        mask <<= 1;
    }
    return result;
}

TASK_3(BDD, sylvan_encode_block, BDD, vars, uint64_t, numberOfVariables, uint64_t, blockIndex) {
    std::vector<uint8_t> e(numberOfVariables);
    for (uint64_t i = 0; i < numberOfVariables; ++i) {
        e[i] = blockIndex & 1 ? 1 : 0;
        blockIndex >>= 1;
    }
    return sylvan_cube(vars, e.data());
}

TASK_3(BDD, sylvan_assign_block, BDD, sig, BDD, previous_block, InternalSylvanSignatureRefinerBase*, refiner) {
    assert(previous_block != mtbdd_false);  // if so, incorrect call!

    // maybe do garbage collection
    sylvan_gc_test();

    if (sig == sylvan_false) {
        // slightly different handling because sylvan_false == 0
        sig = (uint64_t)-1;
    }

    if (refiner->options.reuseBlockNumbers) {
        // try to claim previous block number
        assert(previous_block != sylvan_false);
        const uint64_t p_b = CALL(sylvan_decode_block, previous_block);
        assert(p_b < refiner->signatures.size());

        for (;;) {
            BDD cur = *(volatile BDD*)&refiner->signatures[p_b];
            if (cur == sig)
                return previous_block;
            if (cur != 0)
                break;
            if (cas(&refiner->signatures[p_b], 0, sig))
                return previous_block;
        }
    }

    // no previous block number, search or insert
    uint64_t c;
    while ((c = sylvan_search_or_insert(sig, previous_block, refiner)) == NO_ELEMENT_MARKER) {
        CALL(sylvan_grow, refiner);
    }

    return CALL(sylvan_encode_block, refiner->blockCube.getInternalBdd().getSylvanBdd().GetBDD(), refiner->numberOfBlockVariables, c);
}

TASK_5(BDD, sylvan_refine_partition, BDD, dd, BDD, previous_partition, BDD, nondetvars, BDD, vars, InternalSylvanSignatureRefinerBase*, refiner) {
    /* expecting dd as in s,a,B */
    /* expecting vars to be conjunction of variables in s */
    /* expecting previous_partition as in t,B */

    if (previous_partition == sylvan_false) {
        /* it had no block in the previous iteration, therefore also not now */
        return sylvan_false;
    }

    if (sylvan_set_isempty(vars)) {
        BDD result;
        if (cache_get(dd | (256LL << 42), vars, previous_partition | (refiner->numberOfRefinements << 40), &result))
            return result;
        result = CALL(sylvan_assign_block, dd, previous_partition, refiner);
        cache_put(dd | (256LL << 42), vars, previous_partition | (refiner->numberOfRefinements << 40), result);
        return result;
    }

    sylvan_gc_test();

    /* vars != sylvan_false */
    /* dd cannot be sylvan_true - if vars != sylvan_true, then dd is in a,B */

    BDDVAR dd_var = sylvan_isconst(dd) ? 0xffffffff : sylvan_var(dd);
    BDDVAR pp_var = sylvan_var(previous_partition);
    BDDVAR vars_var = sylvan_var(vars);
    BDDVAR nondetvars_var = sylvan_isconst(nondetvars) ? 0xffffffff : sylvan_var(nondetvars);
    bool nondet = nondetvars_var == vars_var;
    uint64_t offset = (nondet || !refiner->options.shiftStateVariables) ? 0 : 1;

    while (vars_var < dd_var && vars_var + offset < pp_var) {
        vars = sylvan_set_next(vars);
        if (nondet) {
            nondetvars = sylvan_set_next(nondetvars);
        }
        if (sylvan_set_isempty(vars))
            return CALL(sylvan_refine_partition, dd, previous_partition, nondetvars, vars, refiner);
        vars_var = sylvan_var(vars);
        if (nondet) {
            nondetvars_var = sylvan_isconst(nondetvars) ? 0xffffffff : sylvan_var(nondetvars);
            nondet = nondetvars_var == vars_var;
            offset = (nondet || !refiner->options.shiftStateVariables) ? 0 : 1;
        }
    }

    /* Consult cache */
    BDD result;
    if (cache_get(dd | (256LL << 42), vars, previous_partition | (refiner->numberOfRefinements << 40), &result)) {
        return result;
    }

    /* Compute cofactors */
    BDD dd_low, dd_high;
    if (vars_var == dd_var) {
        dd_low = sylvan_low(dd);
        dd_high = sylvan_high(dd);
    } else {
        dd_low = dd_high = dd;
    }

    BDD pp_low, pp_high;
    if (vars_var + offset == pp_var) {
        pp_low = sylvan_low(previous_partition);
        pp_high = sylvan_high(previous_partition);
    } else {
        pp_low = pp_high = previous_partition;
    }

    /* Recursive steps */
    BDD next_vars = sylvan_set_next(vars);
    BDD next_nondetvars = nondet ? sylvan_set_next(nondetvars) : nondetvars;
    bdd_refs_spawn(SPAWN(sylvan_refine_partition, dd_low, pp_low, next_nondetvars, next_vars, refiner));
    BDD high = bdd_refs_push(CALL(sylvan_refine_partition, dd_high, pp_high, next_nondetvars, next_vars, refiner));
    BDD low = bdd_refs_sync(SYNC(sylvan_refine_partition));
    bdd_refs_pop(1);

    /* rename from s to t */
    result = sylvan_makenode(vars_var + offset, low, high);

    /* Write to cache */
    cache_put(dd | (256LL << 42), vars, previous_partition | (refiner->numberOfRefinements << 40), result);
    return result;
}

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#else
InternalSylvanSignatureRefinerBase::InternalSylvanSignatureRefinerBase(storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager,
                                                                       storm::expressions::Variable const& blockVariable,
                                                                       std::set<storm::expressions::Variable> const& stateVariables,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables,
                                                                       InternalSignatureRefinerOptions const& options) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::InternalSignatureRefiner(
    storm::dd::DdManager<storm::dd::DdType::Sylvan> const& manager, storm::expressions::Variable const& blockVariable,
    std::set<storm::expressions::Variable> const& stateVariables, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nondeterminismVariables,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nonBlockVariables, InternalSignatureRefinerOptions const& options)
    : storm::dd::bisimulation::InternalSylvanSignatureRefinerBase(manager, blockVariable, stateVariables, nondeterminismVariables, nonBlockVariables, options) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
Partition<storm::dd::DdType::Sylvan, ValueType> InternalSignatureRefiner<storm::dd::DdType::Sylvan, ValueType>::refine(
    Partition<storm::dd::DdType::Sylvan, ValueType> const& oldPartition, Signature<storm::dd::DdType::Sylvan, ValueType> const& signature) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
#endif

template class InternalSignatureRefiner<storm::dd::DdType::Sylvan, double>;
template class InternalSignatureRefiner<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class InternalSignatureRefiner<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
