#pragma once

#include <optional>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarModelCheckingData.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"
#include "storm/solver/LpSolver.h"

namespace storm {
namespace modelchecker {
namespace cvar {
/*!
 * Solves an LP for Conditional Value-at-Risk on an MDP with a terminal reward objective.
 * @see https://doi.org/10.1145/3209108.3209176 Fig. 4 for a description of the algorithm as implemented (and slightly altered) here.
 */
template<typename SparseMdpModelType>
class SparseCvarHelper {
public:
    explicit SparseCvarHelper(CvarModelCheckingData<SparseMdpModelType> const& modelCheckingData)
        : modelCheckingData(modelCheckingData) {
        // Intentionally left empty.
    }

    typename SparseMdpModelType::ValueType computeCvar(Environment const&) const {
        using ValueType = typename SparseMdpModelType::ValueType;
        STORM_LOG_THROW(!modelCheckingData.candidateThresholds.empty(), storm::exceptions::NotImplementedException,
                        "CVaR model checking requires at least one target reward threshold candidate.");

        std::optional<ValueType> bestValue;
        for (auto const& threshold : modelCheckingData.candidateThresholds) {
            auto thresholdData = createCvarThresholdData(modelCheckingData.targetStates, modelCheckingData.terminalRewards, threshold);
            auto thresholdValue = buildLpForThreshold(thresholdData);
            if (!thresholdValue.has_value()) {
                continue;
            }
            if (!bestValue.has_value()) {
                bestValue = thresholdValue.value();
            } else if ((storm::solver::minimize(modelCheckingData.optimizationDirection) && thresholdValue.value() < bestValue.value()) ||
                       (storm::solver::maximize(modelCheckingData.optimizationDirection) && thresholdValue.value() > bestValue.value())) {
                bestValue = thresholdValue.value();
            }
        }

        STORM_LOG_THROW(bestValue.has_value(), storm::exceptions::UnexpectedException,
                        "CVaR model checking did not find a feasible LP for any threshold candidate.");
        return bestValue.value();
    }

private:
    template<typename ValueType>
    std::optional<ValueType> buildLpForThreshold(CvarThresholdData<ValueType> const& thresholdData) const {
        using RawLpSolver = storm::solver::LpSolver<ValueType, true>;
        using RawLpConstraint = storm::solver::RawLpConstraint<ValueType>;

        auto lpSolverFactory = storm::utility::solver::getLpSolverFactory<ValueType>();
        auto solver = lpSolverFactory->createRaw("cvar");
        solver->setOptimizationDirection(modelCheckingData.optimizationDirection);
        auto backwardTransitions = modelCheckingData.model.getBackwardTransitions();

        std::vector<typename RawLpSolver::Variable> actionFlowVariables;
        actionFlowVariables.reserve(modelCheckingData.transitionMatrix.getRowCount());
        for (uint64_t row = 0; row < modelCheckingData.transitionMatrix.getRowCount(); ++row) {
            actionFlowVariables.push_back(
                solver->addLowerBoundedContinuousVariable("y_" + std::to_string(row), storm::utility::zero<ValueType>()));
        }

        std::vector<std::optional<typename RawLpSolver::Variable> > recurrentFlowVariables(modelCheckingData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            if (modelCheckingData.targetStates[state]) {
                recurrentFlowVariables[state] =
                    solver->addLowerBoundedContinuousVariable("x_" + std::to_string(state), storm::utility::zero<ValueType>());
            }
        }

        std::vector<std::optional<typename RawLpSolver::Variable> > splitFlowVariables(modelCheckingData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            if (thresholdData.targetStatesBelowOrAtThreshold[state]) {
                splitFlowVariables[state] =
                    solver->addLowerBoundedContinuousVariable("xb_" + std::to_string(state), storm::utility::zero<ValueType>(),
                                                              modelCheckingData.terminalRewards[state]);
            }
        }

        solver->update();

        static_cast<void>(splitFlowVariables);

        // Equation (2) from Fig. 4:
        for (uint64_t state = 0; state < modelCheckingData.transitionMatrix.getRowGroupCount(); ++state) {
            auto outgoingActions = modelCheckingData.transitionMatrix.getRowGroupIndices(state);
            auto incomingActions = backwardTransitions.getRow(state);
            uint64_t reservedSize = outgoingActions.size() + incomingActions.getNumberOfEntries() + (recurrentFlowVariables[state].has_value() ? 1 : 0);
            RawLpConstraint constraint(storm::expressions::RelationType::Equal,
                                       state == modelCheckingData.initialState ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>(),
                                       reservedSize);

            for (auto const& incomingAction : incomingActions) {
                constraint.addToLhs(actionFlowVariables[incomingAction.getColumn()], -incomingAction.getValue());
            }
            for (auto const& action : outgoingActions) {
                constraint.addToLhs(actionFlowVariables[action], storm::utility::one<ValueType>());
            }
            if (recurrentFlowVariables[state].has_value()) {
                constraint.addToLhs(recurrentFlowVariables[state].value(), storm::utility::one<ValueType>());
            }

            solver->addConstraint("transient_flow_" + std::to_string(state), constraint);
        }

        // Equation (3):
        RawLpConstraint recurrentConstraint(storm::expressions::RelationType::Equal,
                                            storm::utility::one<ValueType>(),
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
        return solver->getObjectiveValue();
    }

    CvarModelCheckingData<SparseMdpModelType> const& modelCheckingData;
};
} // namespace cvar
} // namespace modelchecker
} // namespace storm
