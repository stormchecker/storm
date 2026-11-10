#include "Assignment.h"

namespace storm {
namespace prism {
Assignment::Assignment(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename,
                       uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), variable(variable), expression(expression) {
    // Intentionally left empty.
}

std::string const& Assignment::getVariableName() const {
    return variable.getName();
}

storm::expressions::Variable const& Assignment::getVariable() const {
    return variable;
}

storm::expressions::Expression const& Assignment::getExpression() const {
    return this->expression;
}

Assignment Assignment::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return Assignment(this->getVariable(), this->getExpression().substitute(substitution).simplify(), this->getFilename(), this->getLineNumber());
}

Assignment Assignment::substituteNonStandardPredicates() const {
    return Assignment(this->getVariable(), this->getExpression().substituteNonStandardPredicates().simplify(), this->getFilename(), this->getLineNumber());
}

bool Assignment::isIdentity() const {
    if (this->expression.isVariable()) {
        STORM_LOG_ASSERT(this->expression.getVariables().size() == 1, "Invalid number of variables.");
        return variable == *(this->expression.getVariables().begin());
    }
    return false;
}

std::ostream& operator<<(std::ostream& stream, Assignment const& assignment) {
    stream << "(" << assignment.getVariableName() << "' = " << assignment.getExpression() << ")";
    return stream;
}

}  // namespace prism
}  // namespace storm
