#include "storm/solver/Z3LpSolver.h"

#include <memory>
#include <numeric>

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/io/file.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearCoefficientVisitor.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

#ifdef STORM_HAVE_Z3

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(std::string const& name, OptimizationDirection const& optDir)
    : LpSolver<ValueType, RawMode>(optDir), isIncremental(false) {
    z3::config config;
    config.set("model", true);
    context = std::make_unique<z3::context>(config);
    solver = std::unique_ptr<z3::optimize>(new z3::optimize(*context));
    expressionAdapter = std::unique_ptr<storm::adapters::Z3ExpressionAdapter>(new storm::adapters::Z3ExpressionAdapter(*this->manager, *context));
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(std::string const& name) : Z3LpSolver(name, OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(OptimizationDirection const& optDir) : Z3LpSolver("", optDir) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver() : Z3LpSolver("", OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::~Z3LpSolver() {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::update() const {
    // Since the model changed, we erase the optimality flag.
    lastCheckObjectiveValue.reset(nullptr);
    lastCheckModel.reset(nullptr);
    this->currentModelHasBeenOptimized = false;
}

template<typename ValueType, bool RawMode>
typename Z3LpSolver<ValueType, RawMode>::Variable Z3LpSolver<ValueType, RawMode>::addVariable(std::string const& name, VariableType const& type,
                                                                                              std::optional<ValueType> const& lowerBound,
                                                                                              std::optional<ValueType> const& upperBound,
                                                                                              ValueType objectiveFunctionCoefficient) {
    STORM_LOG_ASSERT(isIncremental || !this->manager->hasVariable(name), "Variable with name " << name << " already exists.");
    storm::expressions::Variable newVariable = this->declareOrGetExpressionVariable(name, type);
    if (type == VariableType::Binary) {
        solver->add(expressionAdapter->translateExpression(newVariable.getExpression() >= this->manager->integer(0)));
        solver->add(expressionAdapter->translateExpression(newVariable.getExpression() <= this->manager->integer(1)));
    }
    if (lowerBound) {
        solver->add(expressionAdapter->translateExpression(newVariable.getExpression() >= this->manager->rational(*lowerBound)));
    }
    if (upperBound) {
        solver->add(expressionAdapter->translateExpression(newVariable.getExpression() <= this->manager->rational(*upperBound)));
    }
    if (!storm::utility::isZero(objectiveFunctionCoefficient)) {
        optimizationSummands.push_back(this->manager->rational(objectiveFunctionCoefficient) * newVariable);
    }

    if constexpr (RawMode) {
        rawIndexToVariableMap.push_back(newVariable);
        return rawIndexToVariableMap.size() - 1;
    } else {
        return newVariable;
    }
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::addConstraint(std::string const& name, Constraint const& constraint) {
    if constexpr (RawMode) {
        // Generate expression from raw constraint
        STORM_LOG_ASSERT(constraint.lhsVariableIndices.size() == constraint.lhsCoefficients.size(), "Number of variables and coefficients do not match.");
        std::vector<storm::expressions::Expression> lhsSummands;
        lhsSummands.reserve(constraint.lhsVariableIndices.size());
        auto varIt = constraint.lhsVariableIndices.cbegin();
        auto varItEnd = constraint.lhsVariableIndices.cend();
        auto coefIt = constraint.lhsCoefficients.cbegin();
        for (; varIt != varItEnd; ++varIt, ++coefIt) {
            lhsSummands.push_back(rawIndexToVariableMap[*varIt] * this->manager->rational(*coefIt));
        }
        if (lhsSummands.empty()) {
            lhsSummands.push_back(this->manager->rational(storm::utility::zero<ValueType>()));
        }
        storm::expressions::Expression constraintExpr = storm::expressions::makeBinaryRelationExpression(
            storm::expressions::sum(lhsSummands), this->manager->rational(constraint.rhs), constraint.relationType);
        solver->add(expressionAdapter->translateExpression(constraintExpr));
    } else {
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");
        STORM_LOG_THROW(constraint.getOperator() != storm::expressions::OperatorType::NotEqual, storm::exceptions::InvalidArgumentException,
                        "Illegal constraint uses inequality operator.");
        solver->add(expressionAdapter->translateExpression(constraint));
    }
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const& name, Variable indicatorVariable, bool indicatorValue,
                                                            Constraint const& constraint) {
    if constexpr (RawMode) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Indicator constraints not implemented in RawMode.");
    } else {
        // binary variables are encoded as integer variables with domain {0,1}.
        STORM_LOG_THROW(indicatorVariable.hasIntegerType(), storm::exceptions::InvalidArgumentException,
                        "Variable " << indicatorVariable.getName() << " is not binary.");
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");
        STORM_LOG_THROW(constraint.getOperator() != storm::expressions::OperatorType::NotEqual, storm::exceptions::InvalidArgumentException,
                        "Illegal constraint uses inequality operator.");

        storm::expressions::Expression invertedIndicatorVal =
            this->getConstant(indicatorValue ? storm::utility::zero<ValueType>() : storm::utility::one<ValueType>());
        auto indicatorConstraint = (indicatorVariable.getExpression() == invertedIndicatorVal) || constraint;
        solver->add(expressionAdapter->translateExpression(indicatorConstraint));
    }
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::optimize() const {
    // First incorporate all recent changes.
    this->update();

    // Invoke push() as we want to be able to erase the current optimization function after checking
    solver->push();

    storm::expressions::Expression optimizationFunction = this->manager->integer(0);
    // Solve the optimization problem depending on the optimization direction
    if (!optimizationSummands.empty()) {
        optimizationFunction = storm::expressions::sum(optimizationSummands);
    }
    z3::optimize::handle optFuncHandle = this->getOptimizationDirection() == OptimizationDirection::Minimize
                                             ? solver->minimize(expressionAdapter->translateExpression(optimizationFunction))
                                             : solver->maximize(expressionAdapter->translateExpression(optimizationFunction));

    z3::check_result chkRes = solver->check();
    STORM_LOG_THROW(chkRes != z3::unknown, storm::exceptions::InvalidStateException, "Unable to solve LP problem with Z3: Check result is unknown.");

    // We need to store the resulting information at this point. Otherwise, the information would be lost after calling pop() ...

    // Check feasibility
    lastCheckInfeasible = (chkRes == z3::unsat);
    if (lastCheckInfeasible) {
        lastCheckUnbounded = false;
    } else {
        // Get objective result
        lastCheckObjectiveValue = std::make_unique<z3::expr>(solver->upper(optFuncHandle));
        // Check boundedness
        STORM_LOG_ASSERT(lastCheckObjectiveValue->is_app(), "Failed to convert Z3 expression. Encountered unknown expression type.");
        lastCheckUnbounded = (lastCheckObjectiveValue->decl().decl_kind() != Z3_OP_ANUM);
        if (lastCheckUnbounded) {
            lastCheckObjectiveValue.reset(nullptr);
        } else {
            // Assert that the upper approximation equals the lower one
            STORM_LOG_ASSERT(std::string(Z3_get_numeral_string(*context, *lastCheckObjectiveValue)) ==
                                 std::string(Z3_get_numeral_string(*context, solver->lower(optFuncHandle))),
                             "Lower and Upper Approximation of z3LPSolver result do not match.");
            lastCheckModel = std::make_unique<z3::model>(solver->get_model());
        }
    }

    solver->pop();  // removes current optimization function
    this->currentModelHasBeenOptimized = true;
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to Z3LpSolver<ValueType, RawMode>::isInfeasible: model has not been optimized.");
    return lastCheckInfeasible;
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to Z3LpSolver<ValueType, RawMode>::isUnbounded: model has not been optimized.");
    return lastCheckUnbounded;
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isOptimal() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to Z3LpSolver<ValueType, RawMode>::isOptimal: model has not been optimized.");
    return !lastCheckInfeasible && !lastCheckUnbounded;
}

template<typename ValueType, bool RawMode>
storm::expressions::Expression Z3LpSolver<ValueType, RawMode>::getValue(Variable const& variable) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from unoptimized model.");
    }
    STORM_LOG_ASSERT(lastCheckModel, "Model has not been stored.");

    if constexpr (RawMode) {
        STORM_LOG_ASSERT(variable < rawIndexToVariableMap.size(), "Requested variable out of range.");
        z3::expr z3Var = this->expressionAdapter->translateExpression(rawIndexToVariableMap[variable]);
        return this->expressionAdapter->translateExpression(lastCheckModel->eval(z3Var, true));
    } else {
        STORM_LOG_ASSERT(variable.getManager() == this->getManager(), "Requested variable is managed by a different manager.");
        z3::expr z3Var = this->expressionAdapter->translateExpression(variable);
        return this->expressionAdapter->translateExpression(lastCheckModel->eval(z3Var, true));
    }
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable) const {
    storm::expressions::Expression value = getValue(variable);
    if (value.getBaseExpression().isIntegerLiteralExpression()) {
        return storm::utility::convertNumber<ValueType>(value.getBaseExpression().asIntegerLiteralExpression().getValue());
    }
    STORM_LOG_THROW(value.getBaseExpression().isRationalLiteralExpression(), storm::exceptions::ExpressionEvaluationException,
                    "Expected a rational literal while obtaining the value of a continuous variable. Got " << value << "instead.");
    return storm::utility::convertNumber<ValueType>(value.getBaseExpression().asRationalLiteralExpression().getValue());
}

template<typename ValueType, bool RawMode>
int_fast64_t Z3LpSolver<ValueType, RawMode>::getIntegerValue(Variable const& variable) const {
    storm::expressions::Expression value = getValue(variable);
    STORM_LOG_THROW(value.getBaseExpression().isIntegerLiteralExpression(), storm::exceptions::ExpressionEvaluationException,
                    "Expected an integer literal while obtaining the value of an integer variable. Got " << value << "instead.");
    return value.getBaseExpression().asIntegerLiteralExpression().getValue();
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::getBinaryValue(Variable const& variable) const {
    storm::expressions::Expression value = getValue(variable);
    // Binary variables are in fact represented as integer variables!
    STORM_LOG_THROW(value.getBaseExpression().isIntegerLiteralExpression(), storm::exceptions::ExpressionEvaluationException,
                    "Expected an integer literal while obtaining the value of a binary variable. Got " << value << "instead.");
    int_fast64_t val = value.getBaseExpression().asIntegerLiteralExpression().getValue();
    STORM_LOG_THROW((val == 0 || val == 1), storm::exceptions::ExpressionEvaluationException,
                    "Tried to get a binary value for a variable that is neither 0 nor 1.");
    return val == 1;
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getObjectiveValue() const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Z3 solution from unoptimized model.");
    }
    STORM_LOG_ASSERT(lastCheckObjectiveValue, "Objective value has not been stored.");

    storm::expressions::Expression result = this->expressionAdapter->translateExpression(*lastCheckObjectiveValue);
    if (result.getBaseExpression().isIntegerLiteralExpression()) {
        return storm::utility::convertNumber<ValueType>(result.getBaseExpression().asIntegerLiteralExpression().getValue());
    }
    STORM_LOG_THROW(result.getBaseExpression().isRationalLiteralExpression(), storm::exceptions::ExpressionEvaluationException,
                    "Expected a rational literal while obtaining the objective result. Got " << result << "instead.");
    return storm::utility::convertNumber<ValueType>(result.getBaseExpression().asRationalLiteralExpression().getValue());
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    stream << Z3_optimize_to_string(*context, *solver);
    storm::io::closeFile(stream);
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(!RawMode, storm::exceptions::NotImplementedException, "Incremental solving is not supported in Raw mode.");
    incrementaOptimizationSummandIndicators.push_back(optimizationSummands.size());
    solver->push();
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(!RawMode, storm::exceptions::NotImplementedException, "Incremental solving is not supported in Raw mode.");
    STORM_LOG_ASSERT(!incrementaOptimizationSummandIndicators.empty(), "Tried to pop() without push()ing first.");
    solver->pop();
    // Delete summands of the optimization function that have been added since the last call to push()
    optimizationSummands.resize(incrementaOptimizationSummandIndicators.back());
    incrementaOptimizationSummandIndicators.pop_back();
    isIncremental = true;
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const&, bool) {
    // Since the solver is always exact, setting a gap has no effect.
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getMILPGap(bool relative) const {
    // Since the solver is precise, the milp gap is always zero.
    return storm::utility::zero<ValueType>();
}
#else
template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(std::string const&, OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver(OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::Z3LpSolver() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
Z3LpSolver<ValueType, RawMode>::~Z3LpSolver() {}

template<typename ValueType, bool RawMode>
typename Z3LpSolver<ValueType, RawMode>::Variable Z3LpSolver<ValueType, RawMode>::addVariable(std::string const&, VariableType const&,
                                                                                              std::optional<ValueType> const&, std::optional<ValueType> const&,
                                                                                              ValueType) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::update() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::addConstraint(std::string const&, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::optimize() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::isOptimal() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
storm::expressions::Expression Z3LpSolver<ValueType, RawMode>::getValue(Variable const& variable) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getContinuousValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
int_fast64_t Z3LpSolver<ValueType, RawMode>::getIntegerValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
bool Z3LpSolver<ValueType, RawMode>::getBinaryValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getObjectiveValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
void Z3LpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const&, bool) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}

template<typename ValueType, bool RawMode>
ValueType Z3LpSolver<ValueType, RawMode>::getMILPGap(bool relative) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without Z3 or the version of Z3 does not support optimization. Yet, a method was called that "
                    "requires this support.");
}
#endif

template class Z3LpSolver<double, false>;
template class Z3LpSolver<storm::RationalNumber, false>;
template class Z3LpSolver<double, true>;
template class Z3LpSolver<storm::RationalNumber, true>;
}  // namespace solver
}  // namespace storm
