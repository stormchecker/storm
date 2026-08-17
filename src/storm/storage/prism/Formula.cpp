#include "storm/storage/prism/Formula.h"

namespace storm {
namespace prism {
Formula::Formula(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename,
                 uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), name(variable.getName()), variable(variable), expression(expression) {
    // Intentionally left empty.
}

Formula::Formula(std::string const& name, storm::expressions::Expression const& expression, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), name(name), expression(expression) {
    // Intentionally left empty.
}

Formula::Formula(std::string const& name, std::string const& filename, uint_fast64_t lineNumber) : LocatedInformation(filename, lineNumber), name(name) {
    // Intentionally left empty.
}

std::string const& Formula::getName() const {
    return this->name;
}

bool Formula::hasExpressionVariable() const {
    return this->variable.is_initialized();
}

storm::expressions::Variable const& Formula::getExpressionVariable() const {
    return this->variable.get();
}

storm::expressions::Expression const& Formula::getExpression() const {
    return this->expression;
}

storm::expressions::Type const& Formula::getType() const {
    STORM_LOG_ASSERT(this->getExpression().isInitialized(), "Expression not initialized.");
    STORM_LOG_ASSERT(!hasExpressionVariable() || this->getExpressionVariable().getType() == this->getExpression().getType(),
                     "Expression variable type mismatch.");
    return this->getExpressionVariable().getType();
}

Formula Formula::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    STORM_LOG_ASSERT(this->getExpression().isInitialized(), "Expression not initialized.");
    if (hasExpressionVariable()) {
        return Formula(this->getExpressionVariable(), this->getExpression().substitute(substitution), this->getFilename(), this->getLineNumber());
    } else {
        return Formula(this->getName(), this->getExpression().substitute(substitution), this->getFilename(), this->getLineNumber());
    }
}

Formula Formula::substituteNonStandardPredicates() const {
    STORM_LOG_ASSERT(this->getExpression().isInitialized(), "Expression not initialized.");
    if (hasExpressionVariable()) {
        return Formula(this->getExpressionVariable(), this->getExpression().substituteNonStandardPredicates(), this->getFilename(), this->getLineNumber());
    } else {
        return Formula(this->getName(), this->getExpression().substituteNonStandardPredicates(), this->getFilename(), this->getLineNumber());
    }
}

std::ostream& operator<<(std::ostream& stream, Formula const& formula) {
    stream << "formula " << formula.getName() << " = " << formula.getExpression() << ";";
    return stream;
}
}  // namespace prism
}  // namespace storm
