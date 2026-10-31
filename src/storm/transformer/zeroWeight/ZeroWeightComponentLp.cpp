#include "storm/transformer/zeroWeight/ZeroWeightComponentLp.h"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/solver/LpSolver.h"
#include "storm/solver/SoplexLpSolver.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm::transformer {

template<typename ValueType>
struct ZeroWeightComponentLp<ValueType>::SolverSession {
    std::unique_ptr<storm::solver::LpSolver<ValueType>> solver;
    std::vector<storm::expressions::Variable> actionVariables;
    std::vector<storm::expressions::Variable> boundaryVariables;
    std::vector<storm::expressions::Variable> entryVariables;
    storm::expressions::Variable scoreVariable;
};

template<typename ValueType>
ZeroWeightComponentLp<ValueType>::ZeroWeightComponentLp(storm::Environment const& environment, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                        std::vector<ValueType> const& actionWeights, ZeroWeightComponent const& component)
    : environment(environment), states(component.states), entryStates(component.entryStates), boundaryStates(component.boundaryStates) {
    uint64_t const numberOfStates = transitionMatrix.getRowGroupCount();
    STORM_LOG_THROW(transitionMatrix.getColumnCount() == numberOfStates && actionWeights.size() == transitionMatrix.getRowCount(),
                    storm::exceptions::InvalidArgumentException, "Expected a split transition matrix and one weight per row.");
    STORM_LOG_THROW(!states.empty() && !entryStates.empty() && !boundaryStates.empty(), storm::exceptions::InvalidArgumentException,
                    "Expected a zero-weight component with entries and boundary states.");

    std::sort(states.begin(), states.end());
    std::sort(entryStates.begin(), entryStates.end());
    std::sort(boundaryStates.begin(), boundaryStates.end());
    STORM_LOG_THROW(states.back() < numberOfStates && boundaryStates.back() < numberOfStates &&
                        std::adjacent_find(states.begin(), states.end()) == states.end() &&
                        std::adjacent_find(entryStates.begin(), entryStates.end()) == entryStates.end() &&
                        std::adjacent_find(boundaryStates.begin(), boundaryStates.end()) == boundaryStates.end(),
                    storm::exceptions::InvalidArgumentException, "Invalid zero-weight component states or boundary.");
    for (uint64_t entry : entryStates) {
        STORM_LOG_THROW(std::binary_search(states.begin(), states.end(), entry), storm::exceptions::InvalidArgumentException,
                        "A component entry is not a zero-weight state.");
    }
    for (uint64_t boundary : boundaryStates) {
        STORM_LOG_THROW(!std::binary_search(states.begin(), states.end(), boundary), storm::exceptions::InvalidArgumentException,
                        "A component boundary is a zero-weight state.");
    }

    std::unordered_map<uint64_t, uint64_t> localState;
    std::unordered_map<uint64_t, uint64_t> localBoundary;
    localState.reserve(states.size());
    localBoundary.reserve(boundaryStates.size());
    for (uint64_t index = 0; index < states.size(); ++index) {
        localState.emplace(states[index], index);
    }
    for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
        localBoundary.emplace(boundaryStates[index], index);
    }

    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    std::vector<uint64_t> actionSources;
    for (uint64_t state : states) {
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            STORM_LOG_THROW(storm::utility::isZero(actionWeights[row]), storm::exceptions::InvalidArgumentException,
                            "Expected only zero-weight actions inside the component.");
            actionRows.push_back(row);
            actionSources.push_back(state);
        }
    }

    std::set<uint64_t> discoveredBoundary;
    flowCoefficients.resize(states.size());
    boundaryCoefficients.resize(boundaryStates.size());
    for (uint64_t action = 0; action < actionRows.size(); ++action) {
        uint64_t const row = actionRows[action];
        flowCoefficients[localState.at(actionSources[action])][action] += storm::utility::one<ValueType>();
        for (auto const& transition : transitionMatrix.getRow(row)) {
            STORM_LOG_THROW(storm::utility::isNonNegative(transition.getValue()), storm::exceptions::InvalidArgumentException,
                            "Expected nonnegative transition probabilities.");
            if (!storm::utility::isPositive(transition.getValue())) {
                continue;
            }
            uint64_t const successor = transition.getColumn();
            auto const successorState = localState.find(successor);
            if (successorState != localState.end()) {
                flowCoefficients[successorState->second][action] -= transition.getValue();
            } else {
                auto const successorBoundary = localBoundary.find(successor);
                STORM_LOG_THROW(successorBoundary != localBoundary.end(), storm::exceptions::InvalidArgumentException,
                                "The component boundary does not match its transitions.");
                boundaryCoefficients[successorBoundary->second][action] -= transition.getValue();
                discoveredBoundary.insert(successor);
            }
        }
    }
    STORM_LOG_THROW(std::equal(boundaryStates.begin(), boundaryStates.end(), discoveredBoundary.begin(), discoveredBoundary.end()),
                    storm::exceptions::InvalidArgumentException, "The component boundary does not match its transitions.");
}

template<typename ValueType>
ZeroWeightComponentLp<ValueType>::~ZeroWeightComponentLp() = default;

template<typename ValueType>
std::unique_ptr<typename ZeroWeightComponentLp<ValueType>::SolverSession> ZeroWeightComponentLp<ValueType>::buildSolver() const {
    auto result = std::make_unique<SolverSession>();
    result->solver = storm::utility::solver::getLpSolver<ValueType>(environment, "zero_weight_component");
    result->solver->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    auto& solver = *result->solver;
    auto const zero = storm::utility::zero<ValueType>();
    auto const one = storm::utility::one<ValueType>();
    for (uint64_t row : actionRows) {
        result->actionVariables.push_back(solver.addLowerBoundedContinuousVariable("y_" + std::to_string(row), zero));
    }
    for (uint64_t state : boundaryStates) {
        result->boundaryVariables.push_back(solver.addBoundedContinuousVariable("x_" + std::to_string(state), zero, one));
    }
    for (uint64_t state : entryStates) {
        result->entryVariables.push_back(solver.addBoundedContinuousVariable("entry_" + std::to_string(state), zero, one));
    }
    result->scoreVariable = solver.addUnboundedContinuousVariable("score", one);
    solver.update();

    for (uint64_t index = 0; index < states.size(); ++index) {
        auto expression = solver.getConstant(zero);
        for (auto const& [action, coefficient] : flowCoefficients[index]) {
            if (!storm::utility::isZero(coefficient)) {
                expression = expression + result->actionVariables[action].getExpression() * solver.getConstant(coefficient);
            }
        }
        auto const entry = std::lower_bound(entryStates.begin(), entryStates.end(), states[index]);
        if (entry != entryStates.end() && *entry == states[index]) {
            expression = expression - result->entryVariables[entry - entryStates.begin()].getExpression();
        }
        solver.addConstraint("flow_" + std::to_string(states[index]), expression == solver.getConstant(zero));
    }
    for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
        auto expression = result->boundaryVariables[index].getExpression();
        for (auto const& [action, coefficient] : boundaryCoefficients[index]) {
            expression = expression + result->actionVariables[action].getExpression() * solver.getConstant(coefficient);
        }
        solver.addConstraint("exit_" + std::to_string(boundaryStates[index]), expression == solver.getConstant(zero));
    }
    auto totalExit = solver.getConstant(zero);
    for (auto const& variable : result->boundaryVariables) {
        totalExit = totalExit + variable.getExpression();
    }
    solver.addConstraint("total_exit", totalExit == solver.getConstant(one));
    auto totalEntry = solver.getConstant(zero);
    for (auto const& variable : result->entryVariables) {
        totalEntry = totalEntry + variable.getExpression();
    }
    solver.addConstraint("total_entry", totalEntry == solver.getConstant(one));
    return result;
}

template<typename ValueType>
typename ZeroWeightComponentLp<ValueType>::Result ZeroWeightComponentLp<ValueType>::solve(SolverSession& session, uint64_t entryState,
                                                                                          std::vector<ValueType> const& direction, bool reuse,
                                                                                          std::vector<ValueType> const* fixedDirection,
                                                                                          ValueType const* fixedScore) {
    auto& solver = *session.solver;
    if (reuse) {
        solver.push();
    }
    bool needsPop = reuse;
    try {
        auto const entry = std::lower_bound(entryStates.begin(), entryStates.end(), entryState);
        solver.addConstraint("select_entry",
                             session.entryVariables[entry - entryStates.begin()].getExpression() == solver.getConstant(storm::utility::one<ValueType>()));
        auto score = session.scoreVariable.getExpression();
        for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
            if (!storm::utility::isZero(direction[index])) {
                score = score - session.boundaryVariables[index].getExpression() * solver.getConstant(direction[index]);
            }
        }
        solver.addConstraint("score_direction", score == solver.getConstant(storm::utility::zero<ValueType>()));
        if (fixedDirection != nullptr) {
            auto fixed = solver.getConstant(storm::utility::zero<ValueType>());
            for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
                if (!storm::utility::isZero((*fixedDirection)[index])) {
                    fixed = fixed + session.boundaryVariables[index].getExpression() * solver.getConstant((*fixedDirection)[index]);
                }
            }
            solver.addConstraint("fixed_score", fixed == solver.getConstant(*fixedScore));
        }
        solver.optimize();
        STORM_LOG_THROW(solver.isOptimal(), storm::exceptions::UnexpectedException, "The zero-weight component LP has no finite optimum.");
        Result result;
        result.objectiveValue = solver.getObjectiveValue();
        for (auto const& variable : session.boundaryVariables) {
            result.boundaryProbabilities.push_back(solver.getContinuousValue(variable));
        }
        for (auto const& variable : session.actionVariables) {
            result.actionFlows.push_back(solver.getContinuousValue(variable));
        }
        if (needsPop) {
            needsPop = false;
            try {
                solver.pop();
            } catch (...) {
                solverSession.reset();
                throw;
            }
        }
        return result;
    } catch (...) {
        if (needsPop) {
            try {
                solver.pop();
            } catch (...) {
                solverSession.reset();
            }
        }
        throw;
    }
}

template<typename ValueType>
typename ZeroWeightComponentLp<ValueType>::Result ZeroWeightComponentLp<ValueType>::maximize(uint64_t entryState, std::vector<ValueType> const& direction) {
    STORM_LOG_THROW(std::binary_search(entryStates.begin(), entryStates.end(), entryState), storm::exceptions::InvalidArgumentException,
                    "Expected an entry state of the zero-weight component.");
    STORM_LOG_THROW(direction.size() == boundaryStates.size(), storm::exceptions::InvalidArgumentException,
                    "Expected one objective coefficient per component boundary state.");
    if (!solverSession || !supportsReuse) {
        solverSession = buildSolver();
        supportsReuse = dynamic_cast<storm::solver::SoplexLpSolver<ValueType>*>(solverSession->solver.get()) == nullptr;
    }
    return solve(*solverSession, entryState, direction, supportsReuse);
}

template<typename ValueType>
typename ZeroWeightComponentLp<ValueType>::Result ZeroWeightComponentLp<ValueType>::maximizeWithFixedScore(uint64_t entryState,
                                                                                                           std::vector<ValueType> const& direction,
                                                                                                           std::vector<ValueType> const& fixedDirection,
                                                                                                           ValueType const& fixedScore) {
    STORM_LOG_THROW(std::binary_search(entryStates.begin(), entryStates.end(), entryState), storm::exceptions::InvalidArgumentException,
                    "Expected an entry state of the zero-weight component.");
    STORM_LOG_THROW(direction.size() == boundaryStates.size() && fixedDirection.size() == boundaryStates.size(), storm::exceptions::InvalidArgumentException,
                    "Expected one objective coefficient per component boundary state.");
    if (!solverSession || !supportsReuse) {
        solverSession = buildSolver();
        supportsReuse = dynamic_cast<storm::solver::SoplexLpSolver<ValueType>*>(solverSession->solver.get()) == nullptr;
    }
    return solve(*solverSession, entryState, direction, supportsReuse, &fixedDirection, &fixedScore);
}

template<typename ValueType>
std::vector<uint64_t> const& ZeroWeightComponentLp<ValueType>::getStates() const {
    return states;
}

template<typename ValueType>
std::vector<uint64_t> const& ZeroWeightComponentLp<ValueType>::getEntryStates() const {
    return entryStates;
}

template<typename ValueType>
std::vector<uint64_t> const& ZeroWeightComponentLp<ValueType>::getBoundaryStates() const {
    return boundaryStates;
}

template<typename ValueType>
std::vector<uint64_t> const& ZeroWeightComponentLp<ValueType>::getActionRows() const {
    return actionRows;
}

template class ZeroWeightComponentLp<double>;
template class ZeroWeightComponentLp<storm::RationalNumber>;

}  // namespace storm::transformer
