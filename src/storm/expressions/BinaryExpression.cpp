#include "storm/expressions/BinaryExpression.h"

#include "storm/exceptions/InvalidAccessException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
BinaryExpression::BinaryExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& firstOperand,
                                   std::shared_ptr<BaseExpression const> const& secondOperand)
    : BaseExpression(manager, type), firstOperand(firstOperand), secondOperand(secondOperand) {
    // Intentionally left empty.
}

bool BinaryExpression::isFunctionApplication() const {
    return true;
}

bool BinaryExpression::containsVariables() const {
    return this->getFirstOperand()->containsVariables() || this->getSecondOperand()->containsVariables();
}

void BinaryExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    VariableGatheringCache cache;
    gatherVariablesCached(variables, cache);
}

void BinaryExpression::gatherVariablesImpl(std::set<storm::expressions::Variable>& variables, VariableGatheringCache& cache) const {
    this->getFirstOperand()->gatherVariablesCached(variables, cache);
    this->getSecondOperand()->gatherVariablesCached(variables, cache);
}

std::shared_ptr<BaseExpression const> const& BinaryExpression::getFirstOperand() const {
    return this->firstOperand;
}

std::shared_ptr<BaseExpression const> const& BinaryExpression::getSecondOperand() const {
    return this->secondOperand;
}

uint_fast64_t BinaryExpression::getArity() const {
    return 2;
}

std::shared_ptr<BaseExpression const> BinaryExpression::getOperand(uint_fast64_t operandIndex) const {
    STORM_LOG_THROW(operandIndex < 2, storm::exceptions::InvalidAccessException, "Unable to access operand " << operandIndex << " in expression of arity 2.");
    if (operandIndex == 0) {
        return this->getFirstOperand();
    } else {
        return this->getSecondOperand();
    }
}
}  // namespace expressions
}  // namespace storm
