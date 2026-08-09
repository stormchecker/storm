#include "storm-pars/modelchecker/region/monotonicity/AssumptionChecker.h"

#include "storm-pars/utility/ModelInstantiator.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/solver/Z3SmtSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/RationalFunctionToExpression.h"
#include "storm/utility/solver.h"

namespace storm {
namespace analysis {
template<typename ValueType, typename ConstantType>
AssumptionChecker<ValueType, ConstantType>::AssumptionChecker(storage::SparseMatrix<ValueType> matrix) {
    this->matrix = matrix;
    useSamples = false;
}

template<typename ValueType, typename ConstantType>
void AssumptionChecker<ValueType, ConstantType>::initializeCheckingOnSamples(std::shared_ptr<logic::Formula const> formula,
                                                                             std::shared_ptr<models::sparse::Dtmc<ValueType>> model,
                                                                             storage::ParameterRegion<ValueType> region, uint_fast64_t numberOfSamples) {
    // Create sample points
    auto instantiator = utility::ModelInstantiator<models::sparse::Dtmc<ValueType>, models::sparse::Dtmc<ConstantType>>(*model);
    auto matrix = model->getTransitionMatrix();
    std::set<VariableType> variables = models::sparse::getProbabilityParameters(*model);

    for (uint_fast64_t i = 0; i < numberOfSamples; ++i) {
        auto valuation = utility::parametric::Valuation<ValueType>();
        for (auto var : variables) {
            auto lb = region.getLowerBoundary(var.name());
            auto ub = region.getUpperBoundary(var.name());
            // Creates samples between lb and ub, that is: lb, lb + (ub-lb)/(#samples -1), lb + 2* (ub-lb)/(#samples -1), ..., ub
            auto val = std::pair<VariableType, CoefficientType>(var, (lb + utility::convertNumber<CoefficientType>(i / (numberOfSamples - 1)) * (ub - lb)));
            valuation.insert(val);
        }
        models::sparse::Dtmc<ConstantType> sampleModel = instantiator.instantiate(valuation);
        auto checker = modelchecker::SparseDtmcPrctlModelChecker<models::sparse::Dtmc<ConstantType>>(sampleModel);
        std::unique_ptr<modelchecker::CheckResult> checkResult;
        if (formula->isProbabilityOperatorFormula() && formula->asProbabilityOperatorFormula().getSubformula().isUntilFormula()) {
            const modelchecker::CheckTask<logic::UntilFormula, ConstantType> checkTask =
                modelchecker::CheckTask<logic::UntilFormula, ConstantType>(formula->asProbabilityOperatorFormula().getSubformula().asUntilFormula());
            checkResult = checker.computeUntilProbabilities(Environment(), checkTask);
        } else if (formula->isProbabilityOperatorFormula() && formula->asProbabilityOperatorFormula().getSubformula().isEventuallyFormula()) {
            const modelchecker::CheckTask<logic::EventuallyFormula, ConstantType> checkTask =
                modelchecker::CheckTask<logic::EventuallyFormula, ConstantType>(formula->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula());
            checkResult = checker.computeReachabilityProbabilities(Environment(), checkTask);
        } else {
            STORM_LOG_THROW(false, exceptions::NotSupportedException, "Expecting until or eventually formula.");
        }
        auto quantitativeResult = checkResult->asExplicitQuantitativeCheckResult<ConstantType>();
        std::vector<ConstantType> values = quantitativeResult.getValueVector();
        samples.push_back(values);
    }
    useSamples = true;
}

template<typename ValueType, typename ConstantType>
void AssumptionChecker<ValueType, ConstantType>::setSampleValues(std::vector<std::vector<ConstantType>> samples) {
    this->samples = samples;
    useSamples = true;
}

template<typename ValueType, typename ConstantType>
AssumptionChecker<ValueType, ConstantType>::AssumptionChecker(std::shared_ptr<logic::Formula const> formula,
                                                              std::shared_ptr<models::sparse::Mdp<ValueType>> model, uint_fast64_t numberOfSamples) {
    STORM_LOG_THROW(false, exceptions::NotSupportedException, "Assumption checking for mdps not yet implemented.");
}

template<typename ValueType, typename ConstantType>
AssumptionStatus AssumptionChecker<ValueType, ConstantType>::validateAssumption(Assumption const& assumption, std::shared_ptr<Order> order,
                                                                                storage::ParameterRegion<ValueType> region,
                                                                                std::vector<ConstantType> const minValues,
                                                                                std::vector<ConstantType> const maxValues) const {
    // First check if based on sample points the assumption can be discharged
    AssumptionStatus result = AssumptionStatus::UNKNOWN;
    if (useSamples) {
        result = checkOnSamples(assumption);
    }
    STORM_LOG_ASSERT(result != AssumptionStatus::VALID, "Sample-based checking should never conclude VALID by itself.");

    if (minValues.size() != 0) {
        auto const val1 = assumption.state1;
        auto const val2 = assumption.state2;
        if (assumption.relation == expressions::RelationType::Greater) {
            if (minValues[val1] > maxValues[val2]) {
                return AssumptionStatus::VALID;
            } else if (minValues[val1] == maxValues[val2] && minValues[val1] == maxValues[val1] && minValues[val2] == maxValues[val2]) {
                return AssumptionStatus::INVALID;
            } else if (minValues[val2] > maxValues[val1]) {
                return AssumptionStatus::INVALID;
            }
        } else {
            if (minValues[val1] == maxValues[val2] && minValues[val1] == maxValues[val1] && minValues[val2] == maxValues[val2]) {
                return AssumptionStatus::VALID;
            } else if (minValues[val1] > maxValues[val2]) {
                return AssumptionStatus::INVALID;
            } else if (minValues[val2] > maxValues[val1]) {
                return AssumptionStatus::INVALID;
            }
        }
    }

    if (result == AssumptionStatus::UNKNOWN) {
        // If result from sample checking was unknown, the assumption might hold
        STORM_LOG_THROW(assumption.relation == expressions::RelationType::Greater || assumption.relation == expressions::RelationType::Equal,
                        exceptions::NotSupportedException, "Only Greater Or Equal assumptions supported");
        result = validateAssumptionSMTSolver(assumption, order, region, minValues, maxValues);
    }
    return result;
}

template<typename ValueType, typename ConstantType>
AssumptionStatus AssumptionChecker<ValueType, ConstantType>::checkOnSamples(Assumption const& assumption) const {
    STORM_LOG_ASSERT(assumption.relation == expressions::RelationType::Greater || assumption.relation == expressions::RelationType::Equal,
                      "Only Greater or Equal assumptions are supported.");
    auto result = AssumptionStatus::UNKNOWN;
    for (auto const& values : samples) {
        bool holds = assumption.relation == expressions::RelationType::Greater ? values[assumption.state1] > values[assumption.state2]
                                                                               : values[assumption.state1] == values[assumption.state2];
        if (!holds) {
            result = AssumptionStatus::INVALID;
            break;
        }
    }
    return result;
}

template<typename ValueType, typename ConstantType>
AssumptionStatus AssumptionChecker<ValueType, ConstantType>::validateAssumptionSMTSolver(Assumption const& assumption, std::shared_ptr<Order> order,
                                                                                         storage::ParameterRegion<ValueType> region,
                                                                                         std::vector<ConstantType> const minValues,
                                                                                         std::vector<ConstantType> const maxValues) const {
    std::shared_ptr<expressions::ExpressionManager> manager(new expressions::ExpressionManager());
    AssumptionStatus result = AssumptionStatus::UNKNOWN;
    uint_fast64_t val1 = assumption.state1;
    uint_fast64_t val2 = assumption.state2;
    auto row1 = matrix.getRow(val1);
    auto row2 = matrix.getRow(val2);

    bool orderKnown = true;
    // if the state with number val1 (val2) occurs in the successors of the state with number val2 (val1) we need to add val1 == expr1 (val2 == expr2) to the
    // bounds
    bool addVar1 = false;
    bool addVar2 = false;
    // Check if the order between the different successors is known
    // Also start creating expression for order of states
    auto exprOrderSucc = manager->boolean(true);
    std::set<expressions::Variable> stateVariables;
    std::set<expressions::Variable> topVariables;
    std::set<expressions::Variable> bottomVariables;
    for (auto itr1 = row1.begin(); orderKnown && itr1 != row1.end(); ++itr1) {
        addVar2 |= itr1->getColumn() == val2;
        auto varname1 = "s" + std::to_string(itr1->getColumn());
        if (!manager->hasVariable(varname1)) {
            if (order->isTopState(itr1->getColumn())) {
                topVariables.insert(manager->declareRationalVariable(varname1));
            } else if (order->isBottomState(itr1->getColumn())) {
                bottomVariables.insert(manager->declareRationalVariable(varname1));
            } else {
                stateVariables.insert(manager->declareRationalVariable(varname1));
            }
        }

        for (auto itr2 = row2.begin(); orderKnown && itr2 != row2.end(); ++itr2) {
            addVar1 |= itr2->getColumn() == val1;
            if (itr1->getColumn() != itr2->getColumn()) {
                auto varname2 = "s" + std::to_string(itr2->getColumn());
                if (!manager->hasVariable(varname2)) {
                    if (order->isTopState(itr2->getColumn())) {
                        topVariables.insert(manager->declareRationalVariable(varname2));
                    } else if (order->isBottomState(itr2->getColumn())) {
                        bottomVariables.insert(manager->declareRationalVariable(varname2));
                    } else {
                        stateVariables.insert(manager->declareRationalVariable(varname2));
                    }
                }
                auto comp = order->compare(itr1->getColumn(), itr2->getColumn());
                if (minValues.size() > 0 && comp == Order::NodeComparison::UNKNOWN) {
                    // Couldn't add relation between varname1 and varname2 but maybe we can based on min/max values;
                    if (minValues[itr2->getColumn()] > maxValues[itr1->getColumn()]) {
                        if (!order->contains(itr1->getColumn())) {
                            order->add(itr1->getColumn());
                            order->addStateToHandle(itr1->getColumn());
                        }
                        if (!order->contains(itr2->getColumn())) {
                            order->add(itr2->getColumn());
                            order->addStateToHandle(itr2->getColumn());
                        }
                        order->addRelation(itr2->getColumn(), itr1->getColumn());
                        comp = Order::NodeComparison::BELOW;
                    } else if (minValues[itr1->getColumn()] > maxValues[itr2->getColumn()]) {
                        if (!order->contains(itr1->getColumn())) {
                            order->add(itr1->getColumn());
                            order->addStateToHandle(itr1->getColumn());
                        }
                        if (!order->contains(itr2->getColumn())) {
                            order->add(itr2->getColumn());
                            order->addStateToHandle(itr2->getColumn());
                        }
                        order->addRelation(itr1->getColumn(), itr2->getColumn());
                        comp = Order::NodeComparison::ABOVE;
                    }
                }
                if (comp == Order::NodeComparison::ABOVE) {
                    exprOrderSucc = exprOrderSucc && !(manager->getVariable(varname1) <= manager->getVariable(varname2));
                } else if (comp == Order::NodeComparison::BELOW) {
                    exprOrderSucc = exprOrderSucc && !(manager->getVariable(varname1) >= manager->getVariable(varname2));
                } else if (comp == Order::NodeComparison::SAME) {
                    exprOrderSucc = exprOrderSucc && (manager->getVariable(varname1) >= manager->getVariable(varname2)) &&
                                    (manager->getVariable(varname1) <= manager->getVariable(varname2));
                } else {
                    orderKnown = false;
                }
            }
        }
    }

    if (orderKnown) {
        solver::Z3SmtSolver s(*manager);
        auto valueTypeToExpression = expressions::RationalFunctionToExpression<ValueType>(manager);
        expressions::Expression expr1 = manager->rational(0);
        for (auto itr1 = row1.begin(); itr1 != row1.end(); ++itr1) {
            expr1 = expr1 + (valueTypeToExpression.toExpression(itr1->getValue()) * manager->getVariable("s" + std::to_string(itr1->getColumn())));
        }

        expressions::Expression expr2 = manager->rational(0);
        for (auto itr2 = row2.begin(); itr2 != row2.end(); ++itr2) {
            expr2 = expr2 + (valueTypeToExpression.toExpression(itr2->getValue()) * manager->getVariable("s" + std::to_string(itr2->getColumn())));
        }

        // Create expression for the assumption based on the relation to successors
        // It is the negation of actual assumption

        expressions::Expression exprToCheck;
        if (assumption.relation == expressions::RelationType::Greater) {
            exprToCheck = expr1 <= expr2;
        } else {
            STORM_LOG_ASSERT(assumption.relation == expressions::RelationType::Equal, "Only Greater and Equal assumptions are supported.");
            exprToCheck = expr1 != expr2;
        }

        auto variables = manager->getVariables();
        // Bounds for the state probabilities and parameters
        expressions::Expression exprBounds = manager->boolean(true);
        if (addVar1) {
            exprBounds = exprBounds && (manager->getVariable("s" + std::to_string(val1)) == expr1);
        }
        if (addVar2) {
            exprBounds = exprBounds && (manager->getVariable("s" + std::to_string(val2)) == expr2);
        }
        for (auto var : variables) {
            if (find(stateVariables.begin(), stateVariables.end(), var) != stateVariables.end()) {
                // the var is a state
                if (minValues.size() > 0) {
                    std::string test = var.getName();
                    auto val = std::stoi(test.substr(1, test.size() - 1));
                    exprBounds = exprBounds && manager->rational(minValues[val]) <= var && var <= manager->rational(maxValues[val]);
                } else {
                    exprBounds = exprBounds && manager->rational(0) <= var && var <= manager->rational(1);
                }
            } else if (find(topVariables.begin(), topVariables.end(), var) != topVariables.end()) {
                // the var is =)
                exprBounds = exprBounds && var == manager->rational(1);
            } else if (find(bottomVariables.begin(), bottomVariables.end(), var) != bottomVariables.end()) {
                // the var is =(
                exprBounds = exprBounds && var == manager->rational(0);
            } else {
                // the var is a parameter
                auto lb = utility::convertNumber<RationalNumber>(region.getLowerBoundary(var.getName()));
                auto ub = utility::convertNumber<RationalNumber>(region.getUpperBoundary(var.getName()));
                exprBounds = exprBounds && manager->rational(lb) < var && var < manager->rational(ub);
            }
        }

        s.add(exprOrderSucc);
        s.add(exprBounds);
        s.setTimeout(100);
        // assert that sorting of successors in the order and the bounds on the expression are at least satisfiable
        // when this is not the case, the order is invalid
        // however, it could be that the sat solver didn't finish in time, in that case we just continue.
        if (s.check() == solver::SmtSolver::CheckResult::Unsat) {
            return AssumptionStatus::INVALID;
        }

        s.add(exprToCheck);
        auto smtRes = s.check();
        if (smtRes == solver::SmtSolver::CheckResult::Unsat) {
            // If there is no thing satisfying the negation we are safe.
            result = AssumptionStatus::VALID;
        } else if (smtRes == solver::SmtSolver::CheckResult::Sat) {
            result = AssumptionStatus::INVALID;
        }
    }
    return result;
}

template<typename ValueType, typename ConstantType>
AssumptionStatus AssumptionChecker<ValueType, ConstantType>::validateAssumption(Assumption const& assumption, std::shared_ptr<Order> order,
                                                                                storage::ParameterRegion<ValueType> region) const {
    std::vector<ConstantType> vals;
    return validateAssumption(assumption, order, region, vals, vals);
}

template class AssumptionChecker<RationalFunction, double>;
template class AssumptionChecker<RationalFunction, RationalNumber>;
}  // namespace analysis
}  // namespace storm
