#include "Constant.h"

#include "storm/exceptions/InvalidJaniException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace jani {

bool checkDefiningExpression(storm::expressions::Variable const& var, storm::expressions::Expression const& definingExpression,
                             storm::expressions::Expression const& constaintExpression) {
    auto manager = var.getManager().clone();
    auto smtSolver = storm::utility::solver::getSmtSolver(*manager);
    smtSolver->add(var.getExpression().changeManager(*manager) == definingExpression.changeManager(*manager));
    smtSolver->add(constaintExpression.changeManager(*manager));
    return smtSolver->check() == storm::solver::SmtSolver::CheckResult::Sat;
}

Constant::Constant(std::string const& name, storm::expressions::Variable const& variable, storm::expressions::Expression const& definingExpression,
                   storm::expressions::Expression const& constraintExpression)
    : name(name), variable(variable), definingExpression(definingExpression), constraintExpression(constraintExpression) {
    if (definingExpression.isInitialized() && constraintExpression.isInitialized()) {
        STORM_LOG_THROW(checkDefiningExpression(variable, definingExpression, constraintExpression), storm::exceptions::InvalidJaniException,
                        "The constant '" << name << "' was set to '" << definingExpression << "' which violates the constraint given for this constant: '"
                                         << constraintExpression << "'.");
    }
}

std::string const& Constant::getName() const {
    return name;
}

bool Constant::isDefined() const {
    return definingExpression.isInitialized();
}

void Constant::define(storm::expressions::Expression const& expression) {
    if (hasConstraint()) {
        STORM_LOG_THROW(checkDefiningExpression(getExpressionVariable(), expression, constraintExpression), storm::exceptions::InvalidJaniException,
                        "The constant '" << name << "' was set to '" << expression << "' which violates the constraint given for this constant: '"
                                         << constraintExpression << "'.");
    }
    this->definingExpression = expression;
}

storm::expressions::Type const& Constant::getType() const {
    return variable.getType();
}

bool Constant::isBooleanConstant() const {
    return getType().isBooleanType();
}

bool Constant::isIntegerConstant() const {
    return getType().isIntegerType();
}

bool Constant::isRealConstant() const {
    return getType().isRationalType();
}

storm::expressions::Variable const& Constant::getExpressionVariable() const {
    return variable;
}

storm::expressions::Expression const& Constant::getExpression() const {
    STORM_LOG_ASSERT(isDefined(), "Tried to get the constant definition of undefined constant '" << variable.getName() << "'.");
    return definingExpression;
}

bool Constant::hasConstraint() const {
    return constraintExpression.isInitialized();
}

storm::expressions::Expression const& Constant::getConstraintExpression() const {
    STORM_LOG_ASSERT(hasConstraint(), "Tried to get the constant constraint of unconstrained constant '" << variable.getName() << "'.");
    return constraintExpression;
}

void Constant::setConstraintExpression(storm::expressions::Expression const& expression) {
    if (isDefined()) {
        STORM_LOG_THROW(checkDefiningExpression(getExpressionVariable(), definingExpression, expression), storm::exceptions::InvalidJaniException,
                        "The constant '" << name << "' was set to '" << definingExpression << "' which violates the constraint given for this constant: '"
                                         << expression << "'.");
    }
    constraintExpression = expression;
}

}  // namespace jani
}  // namespace storm
