#include "IntegerVariable.h"

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace prism {
IntegerVariable::IntegerVariable(storm::expressions::Variable const& variable, storm::expressions::Expression const& lowerBoundExpression,
                                 storm::expressions::Expression const& upperBoundExpression, storm::expressions::Expression const& initialValueExpression,
                                 bool observable, std::string const& filename, uint_fast64_t lineNumber)
    : Variable(variable, initialValueExpression, observable, filename, lineNumber),
      lowerBoundExpression(lowerBoundExpression),
      upperBoundExpression(upperBoundExpression) {
    // Intentionally left empty.
}

bool IntegerVariable::hasLowerBoundExpression() const {
    return this->lowerBoundExpression.isInitialized();
}

storm::expressions::Expression const& IntegerVariable::getLowerBoundExpression() const {
    STORM_LOG_ASSERT(hasLowerBoundExpression(), "Tried to get the lower bound expression of variable '" << this->getExpressionVariable().getName()
                                                                                                        << "' which is not bounded from below.");
    return this->lowerBoundExpression;
}

bool IntegerVariable::hasUpperBoundExpression() const {
    return this->upperBoundExpression.isInitialized();
}

storm::expressions::Expression const& IntegerVariable::getUpperBoundExpression() const {
    STORM_LOG_ASSERT(hasUpperBoundExpression(), "Tried to get the lower bound expression of variable '" << this->getExpressionVariable().getName()
                                                                                                        << "' which is not bounded from above.");
    return this->upperBoundExpression;
}

storm::expressions::Expression IntegerVariable::getRangeExpression() const {
    if (hasLowerBoundExpression()) {
        if (hasUpperBoundExpression()) {
            return this->getLowerBoundExpression() <= this->getExpressionVariable() && this->getExpressionVariable() <= this->getUpperBoundExpression();
        } else {
            return this->getLowerBoundExpression() <= this->getExpressionVariable();
        }
    } else {
        if (hasUpperBoundExpression()) {
            return this->getExpressionVariable() <= this->getUpperBoundExpression();
        } else {
            return this->getExpressionVariable().getManager().boolean(true);
        }
    }
}

IntegerVariable IntegerVariable::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return IntegerVariable(
        this->getExpressionVariable(),
        this->hasLowerBoundExpression() ? this->getLowerBoundExpression().substitute(substitution) : storm::expressions::Expression(),
        this->hasUpperBoundExpression() ? this->getUpperBoundExpression().substitute(substitution) : storm::expressions::Expression(),
        this->getInitialValueExpression().isInitialized() ? this->getInitialValueExpression().substitute(substitution) : this->getInitialValueExpression(),
        this->isObservable(), this->getFilename(), this->getLineNumber());
}

IntegerVariable IntegerVariable::substituteNonStandardPredicates() const {
    return IntegerVariable(
        this->getExpressionVariable(),
        this->hasLowerBoundExpression() ? this->getLowerBoundExpression().substituteNonStandardPredicates() : storm::expressions::Expression(),
        this->hasUpperBoundExpression() ? this->getUpperBoundExpression().substituteNonStandardPredicates() : storm::expressions::Expression(),
        this->getInitialValueExpression().isInitialized() ? this->getInitialValueExpression().substituteNonStandardPredicates()
                                                          : this->getInitialValueExpression(),
        this->isObservable(), this->getFilename(), this->getLineNumber());
}

void IntegerVariable::createMissingInitialValue() {
    if (!this->hasInitialValue()) {
        if (this->hasLowerBoundExpression()) {
            this->setInitialValueExpression(this->getLowerBoundExpression());
        } else {
            this->setInitialValueExpression(this->getExpressionVariable().getManager().integer(0));
        }
    }
}

std::ostream& operator<<(std::ostream& stream, IntegerVariable const& variable) {
    stream << variable.getName() << ": ";
    if (variable.hasLowerBoundExpression() || variable.hasUpperBoundExpression()) {
        // The syntax for the case where there is only one bound is not standardized, yet.
        stream << "[";
        if (variable.hasLowerBoundExpression()) {
            stream << variable.getLowerBoundExpression();
        }
        stream << "..";
        if (variable.hasUpperBoundExpression()) {
            stream << variable.getUpperBoundExpression();
        }
        stream << "]";
    } else {
        stream << "int";
    }
    if (variable.hasInitialValue()) {
        stream << " init " << variable.getInitialValueExpression();
    }
    stream << ";";
    return stream;
}
}  // namespace prism
}  // namespace storm
