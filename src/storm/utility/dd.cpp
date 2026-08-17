#include "storm/utility/dd.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace utility {
namespace dd {

template<storm::dd::DdType Type>
std::pair<storm::dd::Bdd<Type>, uint64_t> computeReachableStates(storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& transitions,
                                                                 std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                 std::set<storm::expressions::Variable> const& columnMetaVariables) {
    STORM_LOG_TRACE("Computing reachable states: transition matrix BDD has " << transitions.getNodeCount() << " node(s) and " << transitions.getNonZeroCount()
                                                                             << " non-zero(s), " << initialStates.getNonZeroCount() << " initial states).");

    auto start = std::chrono::high_resolution_clock::now();
    storm::dd::Bdd<Type> reachableStates = initialStates;

    // Perform the BFS to discover all reachable states.
    bool changed = true;
    uint_fast64_t iteration = 0;
    do {
        changed = false;
        storm::dd::Bdd<Type> tmp = reachableStates.relationalProduct(transitions, rowMetaVariables, columnMetaVariables);
        storm::dd::Bdd<Type> newReachableStates = tmp && (!reachableStates);

        // Check whether new states were indeed discovered.
        if (!newReachableStates.isZero()) {
            changed = true;
        }

        reachableStates |= newReachableStates;

        ++iteration;
        STORM_LOG_TRACE("Iteration " << iteration << " of reachability computation completed: " << reachableStates.getNonZeroCount()
                                     << " reachable states found.");
    } while (changed);

    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Reachability computation completed in " << iteration << " iterations ("
                                                             << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms).");

    return {reachableStates, iteration};
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> computeBackwardsReachableStates(storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& constraintStates,
                                                     storm::dd::Bdd<Type> const& transitions, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                     std::set<storm::expressions::Variable> const& columnMetaVariables) {
    STORM_LOG_TRACE("Computing backwards reachable states: transition matrix BDD has "
                    << transitions.getNodeCount() << " node(s) and " << transitions.getNonZeroCount() << " non-zero(s), " << initialStates.getNonZeroCount()
                    << " initial states).");

    auto start = std::chrono::high_resolution_clock::now();
    storm::dd::Bdd<Type> reachableStates = initialStates;

    // Perform the BFS to discover all reachable states.
    bool changed = true;
    uint_fast64_t iteration = 0;
    do {
        changed = false;
        storm::dd::Bdd<Type> tmp = reachableStates.inverseRelationalProduct(transitions, rowMetaVariables, columnMetaVariables);
        storm::dd::Bdd<Type> newReachableStates = tmp && (!reachableStates) && constraintStates;

        // Check whether new states were indeed discovered.
        if (!newReachableStates.isZero()) {
            changed = true;
        }

        reachableStates |= newReachableStates;

        ++iteration;
        STORM_LOG_TRACE("Iteration " << iteration << " of (backward) reachability computation completed: " << reachableStates.getNonZeroCount()
                                     << " reachable states found.");
    } while (changed);

    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Backward reachability computation completed in " << iteration << " iterations ("
                                                                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms).");

    return reachableStates;
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> getRowColumnDiagonal(
    storm::dd::DdManager<Type> const& ddManager,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) {
    return ddManager.getIdentity(rowColumnMetaVariablePairs, false);
}

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, uint64_t> computeReachableStates(storm::dd::Bdd<storm::dd::DdType::CUDD> const& initialStates,
                                                                                             storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitions,
                                                                                             std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                                             std::set<storm::expressions::Variable> const& columnMetaVariables);
template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, uint64_t> computeReachableStates(
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& initialStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitions,
    std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables);

template storm::dd::Bdd<storm::dd::DdType::CUDD> computeBackwardsReachableStates(storm::dd::Bdd<storm::dd::DdType::CUDD> const& initialStates,
                                                                                 storm::dd::Bdd<storm::dd::DdType::CUDD> const& constraintStates,
                                                                                 storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitions,
                                                                                 std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                                 std::set<storm::expressions::Variable> const& columnMetaVariables);
template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeBackwardsReachableStates(storm::dd::Bdd<storm::dd::DdType::Sylvan> const& initialStates,
                                                                                   storm::dd::Bdd<storm::dd::DdType::Sylvan> const& constraintStates,
                                                                                   storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitions,
                                                                                   std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                                   std::set<storm::expressions::Variable> const& columnMetaVariables);

template storm::dd::Bdd<storm::dd::DdType::CUDD> getRowColumnDiagonal(
    storm::dd::DdManager<storm::dd::DdType::CUDD> const& ddManager,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs);
template storm::dd::Bdd<storm::dd::DdType::Sylvan> getRowColumnDiagonal(
    storm::dd::DdManager<storm::dd::DdType::Sylvan> const& ddManager,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs);

}  // namespace dd
}  // namespace utility
}  // namespace storm
