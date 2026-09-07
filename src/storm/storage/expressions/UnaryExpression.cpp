#include "UnaryExpression.h"

#include "storm/exceptions/InvalidAccessException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
UnaryExpression::UnaryExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& operand)
    : BaseExpression(manager, type), operand(operand) {
    // Intentionally left empty.
}

bool UnaryExpression::isFunctionApplication() const {
    return true;
}

bool UnaryExpression::containsVariables() const {
    return this->getOperand()->containsVariables();
}

void UnaryExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    this->getOperand()->gatherVariables(variables);
}

std::shared_ptr<BaseExpression const> const& UnaryExpression::getOperand() const {
    return this->operand;
}

uint_fast64_t UnaryExpression::getArity() const {
    return 1;
}

std::shared_ptr<BaseExpression const> UnaryExpression::getOperand(uint_fast64_t operandIndex) const {
    STORM_LOG_THROW(operandIndex == 0, storm::exceptions::InvalidAccessException, "Unable to access operand " << operandIndex << " in expression of arity 2.");
    return this->getOperand();
}
}  // namespace expressions
}  // namespace storm
