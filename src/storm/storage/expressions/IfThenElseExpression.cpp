#include "IfThenElseExpression.h"

#include "storm/exceptions/InvalidAccessException.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
IfThenElseExpression::IfThenElseExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& condition,
                                           std::shared_ptr<BaseExpression const> const& thenExpression,
                                           std::shared_ptr<BaseExpression const> const& elseExpression)
    : BaseExpression(manager, type), condition(condition), thenExpression(thenExpression), elseExpression(elseExpression) {
    // Intentionally left empty.
}

std::shared_ptr<BaseExpression const> IfThenElseExpression::getOperand(uint_fast64_t operandIndex) const {
    STORM_LOG_THROW(operandIndex < 3, storm::exceptions::InvalidAccessException, "Unable to access operand " << operandIndex << " in expression of arity 3.");
    if (operandIndex == 0) {
        return this->getCondition();
    } else if (operandIndex == 1) {
        return this->getThenExpression();
    } else {
        return this->getElseExpression();
    }
}

OperatorType IfThenElseExpression::getOperator() const {
    return OperatorType::Ite;
}

bool IfThenElseExpression::isFunctionApplication() const {
    return true;
}

bool IfThenElseExpression::containsVariables() const {
    return this->getCondition()->containsVariables() || this->getThenExpression()->containsVariables() || this->getElseExpression()->containsVariables();
}

uint_fast64_t IfThenElseExpression::getArity() const {
    return 3;
}

bool IfThenElseExpression::evaluateAsBool(Valuation const* valuation) const {
    bool conditionValue = this->condition->evaluateAsBool(valuation);
    if (conditionValue) {
        return this->thenExpression->evaluateAsBool(valuation);
    } else {
        return this->elseExpression->evaluateAsBool(valuation);
    }
}

int_fast64_t IfThenElseExpression::evaluateAsInt(Valuation const* valuation) const {
    bool conditionValue = this->condition->evaluateAsBool(valuation);
    if (conditionValue) {
        return this->thenExpression->evaluateAsInt(valuation);
    } else {
        return this->elseExpression->evaluateAsInt(valuation);
    }
}

double IfThenElseExpression::evaluateAsDouble(Valuation const* valuation) const {
    bool conditionValue = this->condition->evaluateAsBool(valuation);
    if (conditionValue) {
        return this->thenExpression->evaluateAsDouble(valuation);
    } else {
        return this->elseExpression->evaluateAsDouble(valuation);
    }
}

void IfThenElseExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    this->condition->gatherVariables(variables);
    this->thenExpression->gatherVariables(variables);
    this->elseExpression->gatherVariables(variables);
}

std::shared_ptr<BaseExpression const> IfThenElseExpression::simplify() const {
    std::shared_ptr<BaseExpression const> conditionSimplified = this->condition->simplify();
    if (conditionSimplified->isTrue()) {
        return this->thenExpression->simplify();
    } else if (conditionSimplified->isFalse()) {
        return this->elseExpression->simplify();
    } else {
        std::shared_ptr<BaseExpression const> thenExpressionSimplified = this->thenExpression->simplify();
        std::shared_ptr<BaseExpression const> elseExpressionSimplified = this->elseExpression->simplify();

        if (conditionSimplified.get() == this->condition.get() && thenExpressionSimplified.get() == this->thenExpression.get() &&
            elseExpressionSimplified.get() == this->elseExpression.get()) {
            return this->shared_from_this();
        } else {
            return std::shared_ptr<BaseExpression>(
                new IfThenElseExpression(this->getManager(), this->getType(), conditionSimplified, thenExpressionSimplified, elseExpressionSimplified));
        }
    }
}

boost::any IfThenElseExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool IfThenElseExpression::isIfThenElseExpression() const {
    return true;
}

std::shared_ptr<BaseExpression const> IfThenElseExpression::getCondition() const {
    return this->condition;
}

std::shared_ptr<BaseExpression const> IfThenElseExpression::getThenExpression() const {
    return this->thenExpression;
}

std::shared_ptr<BaseExpression const> IfThenElseExpression::getElseExpression() const {
    return this->elseExpression;
}

void IfThenElseExpression::printToStream(std::ostream& stream) const {
    stream << "(" << *this->condition << " ? " << *this->thenExpression << " : " << *this->elseExpression << ")";
}
}  // namespace expressions
}  // namespace storm
