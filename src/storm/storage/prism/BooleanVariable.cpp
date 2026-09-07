#include "BooleanVariable.h"

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace prism {
BooleanVariable::BooleanVariable(storm::expressions::Variable const& variable, storm::expressions::Expression const& initialValueExpression, bool observable,
                                 std::string const& filename, uint_fast64_t lineNumber)
    : Variable(variable, initialValueExpression, observable, filename, lineNumber) {
    // Nothing to do here.
}

BooleanVariable BooleanVariable::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return BooleanVariable(
        this->getExpressionVariable(),
        this->getInitialValueExpression().isInitialized() ? this->getInitialValueExpression().substitute(substitution) : this->getInitialValueExpression(),
        this->isObservable(), this->getFilename(), this->getLineNumber());
}

BooleanVariable BooleanVariable::substituteNonStandardPredicates() const {
    return BooleanVariable(this->getExpressionVariable(),
                           this->getInitialValueExpression().isInitialized() ? this->getInitialValueExpression().substituteNonStandardPredicates()
                                                                             : this->getInitialValueExpression(),
                           this->isObservable(), this->getFilename(), this->getLineNumber());
}

void BooleanVariable::createMissingInitialValue() {
    if (!this->hasInitialValue()) {
        this->setInitialValueExpression(this->getExpressionVariable().getManager().boolean(false));
    }
}

std::ostream& operator<<(std::ostream& stream, BooleanVariable const& variable) {
    stream << variable.getName() << ": bool";
    if (variable.hasInitialValue()) {
        stream << " init " << variable.getInitialValueExpression();
    }
    stream << ";";
    return stream;
}

}  // namespace prism
}  // namespace storm
