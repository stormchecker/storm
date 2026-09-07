#include "UnaryBooleanFunctionExpression.h"

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/BooleanLiteralExpression.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
UnaryBooleanFunctionExpression::UnaryBooleanFunctionExpression(ExpressionManager const& manager, Type const& type,
                                                               std::shared_ptr<BaseExpression const> const& operand, OperatorType operatorType)
    : UnaryExpression(manager, type, operand), operatorType(operatorType) {
    // Intentionally left empty.
}

UnaryBooleanFunctionExpression::OperatorType UnaryBooleanFunctionExpression::getOperatorType() const {
    return this->operatorType;
}

storm::expressions::OperatorType UnaryBooleanFunctionExpression::getOperator() const {
    storm::expressions::OperatorType result = storm::expressions::OperatorType::Not;
    switch (this->getOperatorType()) {
        case OperatorType::Not:
            result = storm::expressions::OperatorType::Not;
    }
    return result;
}

bool UnaryBooleanFunctionExpression::evaluateAsBool(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasBooleanType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as boolean.");

    bool result = this->getOperand()->evaluateAsBool(valuation);
    switch (this->getOperatorType()) {
        case OperatorType::Not:
            result = !result;
            break;
    }
    return result;
}

std::shared_ptr<BaseExpression const> UnaryBooleanFunctionExpression::simplify() const {
    std::shared_ptr<BaseExpression const> operandSimplified = this->getOperand()->simplify();
    switch (this->getOperatorType()) {
        case OperatorType::Not:
            if (operandSimplified->isTrue()) {
                return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), false));
            } else if (operandSimplified->isFalse()) {
                return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), true));
            }
    }

    if (operandSimplified.get() == this->getOperand().get()) {
        return this->shared_from_this();
    } else {
        return std::shared_ptr<BaseExpression>(
            new UnaryBooleanFunctionExpression(this->getManager(), this->getType(), operandSimplified, this->getOperatorType()));
    }
}

boost::any UnaryBooleanFunctionExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool UnaryBooleanFunctionExpression::isUnaryBooleanFunctionExpression() const {
    return true;
}

void UnaryBooleanFunctionExpression::printToStream(std::ostream& stream) const {
    stream << "!(" << *this->getOperand() << ")";
}
}  // namespace expressions
}  // namespace storm
