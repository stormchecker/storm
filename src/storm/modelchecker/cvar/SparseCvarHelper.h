#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/cvar/CvarModelCheckingData.h"
#include "storm/solver/LpSolver.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
struct CvarComputationResult {
    ValueType value;
    std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;
};
/*!
 * Solves an LP for Conditional Value-at-Risk on an MDP with a terminal reward objective.
 *
 * Supported CLI shape:
 *   storm --prism model.nm --prop 'R{"reward"}min/max=? [ F "target" ]' --cvar <alpha>
 *
 * The --cvar option requires exactly one selected property. That property must be unfiltered and must be an unbounded
 * reward query with an optimization direction (min or max) and an eventually formula F phi whose target phi is a state
 * formula. The reward model may be named explicitly (R{"reward"}...) or omitted if the model has a unique reward model.
 *
 * Requirements: sparse MDP, 0 < alpha < 1, exactly one initial state, state-based terminal rewards,
 * reward 0 on non-target states, and absorbing original target states.
 *
 * @see https://doi.org/10.1145/3209108.3209176 Fig. 4 for a description of the algorithm as implemented (and slightly altered) here.
 */
template<typename ValueType>
class SparseCvarHelper {
   public:
    explicit SparseCvarHelper(CvarModelCheckingData<ValueType> const& modelCheckingData) : modelCheckingData(modelCheckingData) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        STORM_LOG_THROW(!modelCheckingData.candidateThresholds.empty(), storm::exceptions::NotImplementedException,
                        "CVaR model checking requires at least one target reward threshold candidate.");

        // checking all possible threshold values for VaR iteratively. Could possibly improved by e.g. creating a product MDP or similar.
        std::optional<ValueType> bestValue;
        std::unique_ptr<storm::storage::Scheduler<ValueType>> bestScheduler;
        for (auto const& threshold : modelCheckingData.candidateThresholds) {
            auto thresholdData = createCvarThresholdData(modelCheckingData.targetStates, modelCheckingData.terminalRewards, threshold);
            auto thresholdResult = buildLpForThreshold(thresholdData, produceScheduler);
            if (!thresholdResult.has_value()) {
                continue;
            }
            if (!bestValue.has_value()) {
                bestValue = thresholdResult->value;
                if (produceScheduler) {
                    bestScheduler = std::move(thresholdResult->scheduler);
                }
            } else if ((storm::solver::minimize(modelCheckingData.optimizationDirection) && thresholdResult->value < bestValue.value()) ||
                       (storm::solver::maximize(modelCheckingData.optimizationDirection) && thresholdResult->value > bestValue.value())) {
                bestValue = thresholdResult->value;
                if (produceScheduler) {
                    bestScheduler = std::move(thresholdResult->scheduler);
                }
            }
        }

        STORM_LOG_THROW(bestValue.has_value(), storm::exceptions::UnexpectedException,
                        "CVaR model checking did not find a feasible LP for any threshold candidate.");
        return {bestValue.value() / storm::utility::convertNumber<ValueType>(modelCheckingData.alpha), std::move(bestScheduler)};
    }

   private:
    std::optional<CvarComputationResult<ValueType>> buildLpForThreshold(CvarThresholdData<ValueType> const& thresholdData, bool produceScheduler) const {
        using RawLpSolver = storm::solver::LpSolver<ValueType, true>;
        using RawLpConstraint = storm::solver::RawLpConstraint<ValueType>;

        auto lpSolverFactory = storm::utility::solver::getLpSolverFactory<ValueType>();
        auto solver = lpSolverFactory->createRaw("cvar");
        solver->setOptimizationDirection(modelCheckingData.optimizationDirection);
        auto backwardChoices = modelCheckingData.transitionMatrix.transpose();

        std::vector<typename RawLpSolver::Variable> actionFlowVariables;
        actionFlowVariables.reserve(modelCheckingData.transitionMatrix.getRowCount());
        for (uint64_t row = 0; row < modelCheckingData.transitionMatrix.getRowCount(); ++row) {
            actionFlowVariables.push_back(solver->addLowerBoundedContinuousVariable("y_" + std::to_string(row), storm::utility::zero<ValueType>()));
        }

        std::vector<std::optional<typename RawLpSolver::Variable>> recurrentFlowVariables(modelCheckingData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            if (modelCheckingData.targetStates[state]) {
                recurrentFlowVariables[state] = solver->addLowerBoundedContinuousVariable("x_" + std::to_string(state), storm::utility::zero<ValueType>());
            }
        }

        std::vector<std::optional<typename RawLpSolver::Variable>> splitFlowVariables(modelCheckingData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            if (thresholdData.targetStatesBelowOrAtThreshold[state]) {
                splitFlowVariables[state] = solver->addLowerBoundedContinuousVariable("xb_" + std::to_string(state), storm::utility::zero<ValueType>(),
                                                                                      modelCheckingData.terminalRewards[state]);
            }
        }

        solver->update();

        // Equation (2) from Fig. 4:
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            auto outgoingActions = modelCheckingData.transitionMatrix.getRowGroupIndices(state);
            auto incomingActions = backwardChoices.getRow(state);
            uint64_t reservedSize = outgoingActions.size() + incomingActions.getNumberOfEntries() + (recurrentFlowVariables[state].has_value() ? 1 : 0);
            RawLpConstraint constraint(storm::expressions::RelationType::Equal,
                                       state == modelCheckingData.initialState ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>(),
                                       reservedSize);
            std::map<typename RawLpSolver::Variable, ValueType> actionCoefficients;

            for (auto const& incomingAction : incomingActions) {
                actionCoefficients[actionFlowVariables[incomingAction.getColumn()]] -= incomingAction.getValue();
            }
            for (auto const& action : outgoingActions) {
                actionCoefficients[actionFlowVariables[action]] += storm::utility::one<ValueType>();
            }
            for (auto const& actionCoefficient : actionCoefficients) {
                if (!storm::utility::isZero(actionCoefficient.second)) {
                    constraint.addToLhs(actionCoefficient.first, actionCoefficient.second);
                }
            }
            if (recurrentFlowVariables[state].has_value()) {
                constraint.addToLhs(recurrentFlowVariables[state].value(), storm::utility::one<ValueType>());
            }

            solver->addConstraint("transient_flow_" + std::to_string(state), constraint);
        }

        // Equation (3):
        RawLpConstraint recurrentConstraint(storm::expressions::RelationType::Equal, storm::utility::one<ValueType>(),
                                            modelCheckingData.targetStates.getNumberOfSetBits());

        for (auto state : modelCheckingData.targetStates) {
            recurrentConstraint.addToLhs(recurrentFlowVariables[state].value(), storm::utility::one<ValueType>());
        }
        solver->addConstraint("recurrent_behaviour", recurrentConstraint);

        // Equation (4):
        for (auto state : thresholdData.targetStatesBelowThreshold) {
            RawLpConstraint splitEqualityConstraint(storm::expressions::RelationType::Equal, storm::utility::zero<ValueType>(), 2);
            splitEqualityConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
            splitEqualityConstraint.addToLhs(recurrentFlowVariables[state].value(), -storm::utility::one<ValueType>());
            solver->addConstraint("split_eq_" + std::to_string(state), splitEqualityConstraint);
        }
        for (auto state : thresholdData.targetStatesAtThreshold) {
            RawLpConstraint splitInequalityConstraint(storm::expressions::RelationType::LessOrEqual, storm::utility::zero<ValueType>(), 2);
            splitInequalityConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
            splitInequalityConstraint.addToLhs(recurrentFlowVariables[state].value(), -storm::utility::one<ValueType>());
            solver->addConstraint("split_le_" + std::to_string(state), splitInequalityConstraint);
        }

        // Equation (5):
        RawLpConstraint probabilityConsistentSplitConstraint(storm::expressions::RelationType::Equal,
                                                             storm::utility::convertNumber<ValueType>(modelCheckingData.alpha),
                                                             thresholdData.targetStatesBelowOrAtThreshold.getNumberOfSetBits());
        for (auto state : thresholdData.targetStatesBelowOrAtThreshold) {
            probabilityConsistentSplitConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
        }
        solver->addConstraint("probability_consistent_split", probabilityConsistentSplitConstraint);

        solver->optimize();
        if (solver->isInfeasible()) {
            return std::nullopt;
        }
        STORM_LOG_THROW(!solver->isUnbounded(), storm::exceptions::UnexpectedException,
                        "The CVaR LP for threshold " << thresholdData.threshold << " is unbounded.");
        STORM_LOG_THROW(solver->isOptimal(), storm::exceptions::UnexpectedException,
                        "The CVaR LP for threshold " << thresholdData.threshold << " did not reach an optimal solution.");

        std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;
        if (produceScheduler) {
            scheduler = std::make_unique<storm::storage::Scheduler<ValueType>>(modelCheckingData.transitionMatrix.getRowGroupCount());
            for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
                if (modelCheckingData.targetStates[state]) {
                    scheduler->setDontCare(state);
                    continue;
                }

                uint64_t firstRow = modelCheckingData.transitionMatrix.getRowGroupIndices()[state];
                uint64_t lastRow = modelCheckingData.transitionMatrix.getRowGroupIndices()[state + 1];
                storm::storage::Distribution<ValueType, uint_fast64_t> actionDistribution;
                actionDistribution.reserve(lastRow - firstRow);

                for (uint64_t row = firstRow; row < lastRow; ++row) {
                    auto flow = solver->getContinuousValue(actionFlowVariables[row]);
                    if (storm::utility::isAlmostZero(flow)) {
                        continue;
                    }
                    actionDistribution.addProbability(row - firstRow, flow);
                }

                if (actionDistribution.size() == 0) {
                    scheduler->setDontCare(state);
                    continue;
                }

                actionDistribution.normalize();
                scheduler->setChoice(storm::storage::SchedulerChoice<ValueType>(std::move(actionDistribution)), state);
            }
        }

        return CvarComputationResult<ValueType>{solver->getObjectiveValue(), std::move(scheduler)};
    }

    CvarModelCheckingData<ValueType> const& modelCheckingData;
};
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
