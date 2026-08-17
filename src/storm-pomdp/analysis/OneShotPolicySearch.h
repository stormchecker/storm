#pragma once

#include <vector>

#include "storm/models/sparse/Pomdp.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/solver.h"

namespace storm {
namespace pomdp {

template<typename ValueType>
std::set<uint32_t> extractObservations(storm::models::sparse::Pomdp<ValueType> const& pomdp, storm::storage::BitVector const& states) {
    // TODO move.
    std::set<uint32_t> observations;
    for (auto state : states) {
        observations.insert(pomdp.getObservation(state));
    }
    return observations;
}

template<typename ValueType>
class OneShotPolicySearch {
    /*!
     *  Implements  to the Chatterjee, Chmelik, Davies (AAAI-16) paper.
     *  Tries to find a memoryless policy.
     */
    class Statistics {
       public:
        Statistics() = default;

        storm::utility::Stopwatch totalTimer;
        storm::utility::Stopwatch smtCheckTimer;
        storm::utility::Stopwatch initializeSolverTimer;
    };

   public:
    OneShotPolicySearch(storm::models::sparse::Pomdp<ValueType> const& pomdp, storm::storage::BitVector const& targetStates,
                        storm::storage::BitVector const& surelyReachSinkStates, std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory)
        : pomdp(pomdp), targetObservations(extractObservations(pomdp, targetStates)), targetStates(targetStates), surelyReachSinkStates(surelyReachSinkStates) {
        this->expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
        smtSolver = smtSolverFactory->create(*expressionManager);
    }

    void setSurelyReachSinkStates(storm::storage::BitVector const& surelyReachSink) {
        surelyReachSinkStates = surelyReachSink;
    }

    /*!
     * Check if you can find a memoryless policy from the initial states
     * @param k The used lookahed
     * @return Replies true, if a memoryless policy is found. Notice that the algorithm is not complete.
     */
    bool analyzeForInitialStates(uint64_t k) {
        STORM_LOG_TRACE("Bad states: " << surelyReachSinkStates);
        STORM_LOG_TRACE("Target states: " << targetStates);
        STORM_LOG_TRACE("Questionmark states: " << (~surelyReachSinkStates & ~targetStates));
        return analyze(k, ~surelyReachSinkStates & ~targetStates, pomdp.getInitialStates());
    }

   private:
    bool analyze(uint64_t k, storm::storage::BitVector const& oneOfTheseStates,
                 storm::storage::BitVector const& allOfTheseStates = storm::storage::BitVector());

    void initialize(uint64_t k);

    Statistics stats;
    std::unique_ptr<storm::solver::SmtSolver> smtSolver;
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;
    uint64_t maxK = std::numeric_limits<uint64_t>::max();

    std::set<uint32_t> targetObservations;
    storm::storage::BitVector targetStates;
    storm::storage::BitVector surelyReachSinkStates;

    std::vector<std::vector<uint64_t>> statesPerObservation;
    std::vector<std::vector<storm::expressions::Expression>> actionSelectionVarExpressions;  // A_{z,a}
    std::vector<std::vector<storm::expressions::Variable>> actionSelectionVars;
    std::vector<storm::expressions::Variable> reachVars;
    std::vector<storm::expressions::Expression> reachVarExpressions;
    std::vector<std::vector<storm::expressions::Expression>> pathVars;
};
}  // namespace pomdp
}  // namespace storm
