#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace analysis {

template<typename ValueType>
QualitativeAnalysisOnGraphs<ValueType>::QualitativeAnalysisOnGraphs(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {
    // Intentionally left empty
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb0(storm::logic::ProbabilityOperatorFormula const& formula) const {
    return analyseProb0or1(formula, true);
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb1(storm::logic::ProbabilityOperatorFormula const& formula) const {
    return analyseProb0or1(formula, false);
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProbSmaller1(storm::logic::ProbabilityOperatorFormula const& formula) const {
    STORM_LOG_THROW(formula.hasOptimalityType() || formula.hasBound(), storm::exceptions::InvalidPropertyException,
                    "The formula " << formula << " does not specify whether to minimize or maximize.");
    bool minimizes = (formula.hasOptimalityType() && storm::solver::minimize(formula.getOptimalityType())) ||
                     (formula.hasBound() && storm::logic::isLowerBound(formula.getBound().comparisonType));
    STORM_LOG_THROW(!minimizes, storm::exceptions::NotImplementedException, "This operation is only supported when maximizing.");
    std::shared_ptr<storm::logic::Formula const> subformula = formula.getSubformula().asSharedPointer();
    std::shared_ptr<storm::logic::UntilFormula> untilSubformula;
    // If necessary, convert the subformula to a more general case
    if (subformula->isEventuallyFormula()) {
        untilSubformula = std::make_shared<storm::logic::UntilFormula>(storm::logic::Formula::getTrueFormula(),
                                                                       subformula->asEventuallyFormula().getSubformula().asSharedPointer());
    } else if (subformula->isUntilFormula()) {
        untilSubformula = std::make_shared<storm::logic::UntilFormula>(subformula->asUntilFormula());
    }
    // The vector is sound, but not necessarily complete!
    return ~storm::utility::graph::performProb1E(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), pomdp.getBackwardTransitions(),
                                                 checkPropositionalFormula(untilSubformula->getLeftSubformula()),
                                                 checkPropositionalFormula(untilSubformula->getRightSubformula()));
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb0or1(storm::logic::ProbabilityOperatorFormula const& formula, bool prob0) const {
    // check whether the property is minimizing or maximizing
    STORM_LOG_THROW(pomdp.isCanonic(), storm::exceptions::IllegalArgumentException, "POMDP needs to be canonic.");
    STORM_LOG_THROW(formula.hasOptimalityType() || formula.hasBound(), storm::exceptions::InvalidPropertyException,
                    "The formula " << formula << " does not specify whether to minimize or maximize.");
    bool minimizes = (formula.hasOptimalityType() && storm::solver::minimize(formula.getOptimalityType())) ||
                     (formula.hasBound() && storm::logic::isLowerBound(formula.getBound().comparisonType));

    std::shared_ptr<storm::logic::Formula const> subformula = formula.getSubformula().asSharedPointer();
    // If necessary, convert the subformula to a more general case
    if (subformula->isEventuallyFormula()) {
        subformula = std::make_shared<storm::logic::UntilFormula>(storm::logic::Formula::getTrueFormula(),
                                                                  subformula->asEventuallyFormula().getSubformula().asSharedPointer());
    }

    if (subformula->isUntilFormula()) {
        if (minimizes && prob0) {
            return analyseProb0Min(subformula->asUntilFormula());
        } else if (minimizes && !prob0) {
            return analyseProb1Min(subformula->asUntilFormula());
        } else if (!minimizes && prob0) {
            return analyseProb0Max(subformula->asUntilFormula());
        } else if (!minimizes && !prob0) {
            return analyseProb1Max(subformula->asUntilFormula());
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Prob0or1 analysis is not supported for the property " << formula << ".");
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb0Max(storm::logic::UntilFormula const& formula) const {
    return storm::utility::graph::performProb0A(pomdp.getBackwardTransitions(), checkPropositionalFormula(formula.getLeftSubformula()),
                                                checkPropositionalFormula(formula.getRightSubformula()));
}

template<typename ValueType>
[[maybe_unused]] storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb0Min(storm::logic::UntilFormula const& formula) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Prob0 analysis is currently not implemented for minimizing properties.");
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb1Max(storm::storage::BitVector const& okay,
                                                                                  storm::storage::BitVector const& good) const {
    storm::storage::BitVector newGoalStates = storm::utility::graph::performProb1A(
        pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), pomdp.getBackwardTransitions(), okay, good);
    STORM_LOG_TRACE("Prob1A states according to MDP: " << newGoalStates);
    // Now find a set of observations such that there is (a memoryless) scheduler inducing prob. 1 for each state whose observation is in the set.
    storm::storage::BitVector potentialGoalStates = storm::utility::graph::performProb1E(
        pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), pomdp.getBackwardTransitions(), okay, newGoalStates);
    STORM_LOG_TRACE("Prob1E states according to MDP: " << potentialGoalStates);

    storm::storage::BitVector avoidStates = ~potentialGoalStates;
    storm::storage::BitVector potentialGoalObservations(pomdp.getNrObservations(), true);
    for (uint64_t observation = 0; observation < pomdp.getNrObservations(); ++observation) {
        for (uint64_t state : avoidStates) {
            potentialGoalObservations.set(pomdp.getObservation(state), false);
        }
    }
    // Prob1E observations are observations from which every state is in Prob1E in the underlying MDP
    STORM_LOG_TRACE("Prob1E observations according to MDP: " << potentialGoalObservations);

    std::vector<std::vector<uint64_t>> statesPerObservation(pomdp.getNrObservations(), std::vector<uint64_t>());
    for (uint64_t state : potentialGoalStates) {
        statesPerObservation[pomdp.getObservation(state)].push_back(state);
    }
    storm::storage::BitVector singleObservationStates(pomdp.getNumberOfStates());
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        if (statesPerObservation[pomdp.getObservation(state)].size() == 1) {
            singleObservationStates.set(state);
        }
    }

    storm::storage::BitVector goalStates(pomdp.getNumberOfStates());
    while (goalStates != newGoalStates) {
        goalStates = storm::utility::graph::performProb1A(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(),
                                                          pomdp.getBackwardTransitions(), okay, newGoalStates);
        goalStates = storm::utility::graph::performProb1E(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(),
                                                          pomdp.getBackwardTransitions(), okay & singleObservationStates, goalStates);
        newGoalStates = goalStates;
        STORM_LOG_TRACE("Prob1A states according to MDP: " << newGoalStates);
        for (uint64_t observation : potentialGoalObservations) {
            STORM_LOG_TRACE("Consider observation " << observation);
            uint64_t actsForObservation = pomdp.getTransitionMatrix().getRowGroupSize(statesPerObservation[observation][0]);
            // Search whether we find an action that works for this observation.
            for (uint64_t act = 0; act < actsForObservation; act++) {
                STORM_LOG_TRACE("Consider action " << act);
                bool isGoalAction = true;  // Assume that this works, then check whether we find a violation.
                for (uint64_t state : statesPerObservation[observation]) {
                    STORM_LOG_TRACE("Consider state " << state);
                    if (newGoalStates.get(state)) {
                        STORM_LOG_TRACE("Already a goal state " << state);

                        // A state is only a goal state if all actions work,
                        // or if all states with the same observation are goal states (and then, it does not matter which action is a goal action).
                        // Notice that this can mean that we wrongly conclude that some action is okay even if this is not the correct action
                        // (but then some other action exists which is okay for all states).
                        continue;
                    }
                    uint64_t row = pomdp.getNondeterministicChoiceIndices()[state] + act;
                    // Check whether all actions lead with a positive probabilty to a goal, and with zero probability to another (non-goal) state.
                    bool hasGoalEntry = false;
                    for (auto const& entry : pomdp.getTransitionMatrix().getRow(row)) {
                        assert(!storm::utility::isZero(entry.getValue()));
                        if (newGoalStates.get(entry.getColumn())) {
                            STORM_LOG_TRACE("Reaches state " << entry.getColumn() << " which is a PROB1e state");
                            hasGoalEntry = true;
                        } else if (pomdp.getObservation(entry.getColumn()) != observation) {
                            STORM_LOG_TRACE("Reaches state " << entry.getColumn() << " which may not be a PROB1e state");
                            isGoalAction = false;
                            break;
                        }
                    }
                    if (!isGoalAction || !hasGoalEntry) {
                        // Action is definitiely not a goal action, no need to check further states.
                        isGoalAction = false;
                        break;
                    }
                }
                if (isGoalAction) {
                    for (uint64_t state : statesPerObservation[observation]) {
                        newGoalStates.set(state);
                    }
                    // No need to check other actions.
                    break;
                }
            }
        }
    }
    // Notice that the goal states are not observable, i.e., the observations may not be sufficient to decide whether we are in a goal state.
    return goalStates;
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb1Max(storm::logic::UntilFormula const& formula) const {
    // We consider the states that satisfy the formula with prob.1 under arbitrary schedulers as goal states.
    return this->analyseProb1Max(checkPropositionalFormula(formula.getLeftSubformula()), checkPropositionalFormula(formula.getRightSubformula()));
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::analyseProb1Min(storm::logic::UntilFormula const& formula) const {
    return storm::utility::graph::performProb1A(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), pomdp.getBackwardTransitions(),
                                                checkPropositionalFormula(formula.getLeftSubformula()),
                                                checkPropositionalFormula(formula.getRightSubformula()));
}

template<typename ValueType>
storm::storage::BitVector QualitativeAnalysisOnGraphs<ValueType>::checkPropositionalFormula(storm::logic::Formula const& propositionalFormula) const {
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Mdp<ValueType>> mc(pomdp);
    STORM_LOG_THROW(mc.canHandle(propositionalFormula), storm::exceptions::InvalidPropertyException,
                    "Propositional model checker can not handle formula " << propositionalFormula << ".");
    return mc.check(propositionalFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
}

template class QualitativeAnalysisOnGraphs<double>;
template class QualitativeAnalysisOnGraphs<storm::RationalNumber>;
}  // namespace analysis
}  // namespace storm