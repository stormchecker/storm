#include "storm/io/file.h"

#include "storm-pomdp/analysis/OneShotPolicySearch.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace pomdp {

template<typename ValueType>
void OneShotPolicySearch<ValueType>::initialize(uint64_t k) {
    if (maxK == std::numeric_limits<uint64_t>::max()) {
        // not initialized at all.
        // Create some data structures.
        for (uint64_t obs = 0; obs < pomdp.getNrObservations(); ++obs) {
            actionSelectionVars.push_back(std::vector<storm::expressions::Variable>());
            actionSelectionVarExpressions.push_back(std::vector<storm::expressions::Expression>());
            statesPerObservation.push_back(std::vector<uint64_t>());  // Consider using bitvectors instead.
        }

        // Fill the states-per-observation mapping,
        // declare the reachability variables,
        // declare the path variables.
        uint64_t stateId = 0;
        for (auto obs : pomdp.getObservations()) {
            pathVars.push_back(std::vector<storm::expressions::Expression>());
            for (uint64_t i = 0; i < k; ++i) {
                pathVars.back().push_back(expressionManager->declareBooleanVariable("P-" + std::to_string(stateId) + "-" + std::to_string(i)).getExpression());
            }
            reachVars.push_back(expressionManager->declareBooleanVariable("C-" + std::to_string(stateId)));
            reachVarExpressions.push_back(reachVars.back().getExpression());
            statesPerObservation.at(obs).push_back(stateId++);
        }
        STORM_LOG_ASSERT(pathVars.size() == pomdp.getNumberOfStates(), "Path vars size mismatch.");

        // Create the action selection variables.
        uint64_t obs = 0;
        for (auto const& statesForObservation : statesPerObservation) {
            for (uint64_t a = 0; a < pomdp.getNumberOfChoices(statesForObservation.front()); ++a) {
                std::string varName = "A-" + std::to_string(obs) + "-" + std::to_string(a);
                actionSelectionVars.at(obs).push_back(expressionManager->declareBooleanVariable(varName));
                actionSelectionVarExpressions.at(obs).push_back(actionSelectionVars.at(obs).back().getExpression());
            }
            ++obs;
        }
    } else {
        STORM_LOG_ASSERT(false, "Unreachable code reached.");
    }

    for (auto const& actionVars : actionSelectionVarExpressions) {
        smtSolver->add(storm::expressions::disjunction(actionVars));
    }

    uint64_t rowindex = 0;
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
            std::vector<storm::expressions::Expression> subexprreach;
            subexprreach.push_back(!reachVarExpressions[state]);
            subexprreach.push_back(!actionSelectionVarExpressions[pomdp.getObservation(state)][action]);
            for (auto const& entries : pomdp.getTransitionMatrix().getRow(rowindex)) {
                subexprreach.push_back(reachVarExpressions.at(entries.getColumn()));
                smtSolver->add(storm::expressions::disjunction(subexprreach));
                subexprreach.pop_back();
            }
            rowindex++;
        }
    }

    rowindex = 0;
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        if (targetStates.get(state)) {
            smtSolver->add(pathVars[state][0]);
        } else {
            smtSolver->add(!pathVars[state][0]);
        }

        if (surelyReachSinkStates.get(state)) {
            smtSolver->add(!reachVarExpressions[state]);
            rowindex += pomdp.getNumberOfChoices(state);
        } else if (!targetStates.get(state)) {
            std::vector<std::vector<std::vector<storm::expressions::Expression>>> pathsubsubexprs;
            for (uint64_t j = 1; j < k; ++j) {
                pathsubsubexprs.push_back(std::vector<std::vector<storm::expressions::Expression>>());
                for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
                    pathsubsubexprs.back().push_back(std::vector<storm::expressions::Expression>());
                }
            }

            for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
                std::vector<storm::expressions::Expression> subexprreach;
                for (auto const& entries : pomdp.getTransitionMatrix().getRow(rowindex)) {
                    for (uint64_t j = 1; j < k; ++j) {
                        pathsubsubexprs[j - 1][action].push_back(pathVars[entries.getColumn()][j - 1]);
                    }
                }
                rowindex++;
            }

            for (uint64_t j = 1; j < k; ++j) {
                std::vector<storm::expressions::Expression> pathsubexprs;

                for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
                    pathsubexprs.push_back(actionSelectionVarExpressions.at(pomdp.getObservation(state)).at(action) &&
                                           storm::expressions::disjunction(pathsubsubexprs[j - 1][action]));
                }
                smtSolver->add(storm::expressions::iff(pathVars[state][j], storm::expressions::disjunction(pathsubexprs)));
            }

            smtSolver->add(storm::expressions::implies(reachVarExpressions.at(state), pathVars.at(state).back()));

        } else {
            rowindex += pomdp.getNumberOfChoices(state);
        }
    }
}

template<typename ValueType>
bool OneShotPolicySearch<ValueType>::analyze(uint64_t k, storm::storage::BitVector const& oneOfTheseStates, storm::storage::BitVector const& allOfTheseStates) {
    STORM_LOG_TRACE("Use lookahead of " << k);
    if (k < maxK) {
        initialize(k);
    }

    std::vector<storm::expressions::Expression> atLeastOneOfStates;

    for (uint64_t state : oneOfTheseStates) {
        atLeastOneOfStates.push_back(reachVarExpressions[state]);
    }
    STORM_LOG_ASSERT(atLeastOneOfStates.size() > 0, "At least one state expected.");
    smtSolver->add(storm::expressions::disjunction(atLeastOneOfStates));

    for (uint64_t state : allOfTheseStates) {
        smtSolver->add(reachVarExpressions[state]);
    }

    STORM_LOG_TRACE(smtSolver->getSmtLibString());

    STORM_LOG_DEBUG("Call to SMT Solver");
    stats.smtCheckTimer.start();
    auto result = smtSolver->check();
    stats.smtCheckTimer.stop();

    if (result == storm::solver::SmtSolver::CheckResult::Unknown) {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "SMT solver yielded an unexpected result.");
    } else if (result == storm::solver::SmtSolver::CheckResult::Unsat) {
        STORM_LOG_DEBUG("Unsatisfiable!");
        return false;
    }

    STORM_LOG_DEBUG("Satisfying assignment: ");
    auto model = smtSolver->getModel();
    size_t i = 0;
    storm::storage::BitVector observations(pomdp.getNrObservations());
    storm::storage::BitVector remainingstates(pomdp.getNumberOfStates());
    for (auto rv : reachVars) {
        if (model->getBooleanValue(rv)) {
            observations.set(pomdp.getObservation(i));
        } else {
            remainingstates.set(i);
        }
        ++i;
    }
    std::vector<std::set<uint64_t>> scheduler;
    for (auto const& actionSelectionVarsForObs : actionSelectionVars) {
        uint64_t act = 0;
        scheduler.push_back(std::set<uint64_t>());
        for (auto const& asv : actionSelectionVarsForObs) {
            if (model->getBooleanValue(asv)) {
                scheduler.back().insert(act);
            }
            act++;
        }
    }

    return true;
}

template class OneShotPolicySearch<double>;
template class OneShotPolicySearch<storm::RationalNumber>;
}  // namespace pomdp
}  // namespace storm
