#include "Constant.h"

#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace prism {
Constant::Constant(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename,
                   uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), variable(variable), expression(expression) {
    // Intentionally left empty.
}

Constant::Constant(storm::expressions::Variable const& variable, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), variable(variable), expression() {
    // Intentionally left empty.
}

std::string const& Constant::getName() const {
    return this->variable.getName();
}

storm::expressions::Type const& Constant::getType() const {
    return this->getExpressionVariable().getType();
}

storm::expressions::Variable const& Constant::getExpressionVariable() const {
    return this->variable;
}

bool Constant::isDefined() const {
    return this->expression.isInitialized();
}

storm::expressions::Expression const& Constant::getExpression() const {
    return this->expression;
}

Constant Constant::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    if (!this->isDefined()) {
        return *this;
    } else {
        return Constant(variable, this->getExpression().substitute(substitution), this->getFilename(), this->getLineNumber());
    }
}

std::ostream& operator<<(std::ostream& stream, Constant const& constant) {
    stream << "const ";
    if (constant.getType().isRationalType()) {
        stream << "double" << " ";
    } else {
        stream << constant.getType() << " ";
    }
    stream << constant.getName();
    if (constant.isDefined()) {
        stream << " = " << constant.getExpression();
    }
    stream << ";";
    return stream;
}

bool operator==(Constant const& lhs, Constant const& rhs) {
    return (lhs.getExpressionVariable() == rhs.getExpressionVariable()) && (lhs.getExpression().isSyntacticallyEqual(rhs.getExpression()));
}
}  // namespace prism
}  // namespace storm
