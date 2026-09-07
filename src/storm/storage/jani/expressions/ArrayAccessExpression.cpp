#include "ArrayAccessExpression.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

ArrayAccessExpression::ArrayAccessExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& arrayExpression,
                                             std::shared_ptr<BaseExpression const> const& indexExpression)
    : BinaryExpression(manager, type, arrayExpression, indexExpression) {
    // Assert correct types
    STORM_LOG_ASSERT(getFirstOperand()->getType().isArrayType(), "ArrayAccessExpression for an expression of type " << getFirstOperand()->getType() << ".");
    STORM_LOG_ASSERT(type == getFirstOperand()->getType().getElementType(),
                     "The ArrayAccessExpression should have type " << getFirstOperand()->getType().getElementType() << " but has " << type << " instead.");
    STORM_LOG_ASSERT(getSecondOperand()->getType().isIntegerType(), "The index expression does not have an integer type.");
}

std::shared_ptr<BaseExpression const> ArrayAccessExpression::simplify() const {
    return std::shared_ptr<BaseExpression const>(
        new ArrayAccessExpression(getManager(), getType(), getFirstOperand()->simplify(), getSecondOperand()->simplify()));
}

boost::any ArrayAccessExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    auto janiVisitor = dynamic_cast<JaniExpressionVisitor*>(&visitor);
    STORM_LOG_ASSERT(janiVisitor != nullptr, "Visitor of jani expression should be of type JaniVisitor.");
    STORM_LOG_THROW(janiVisitor != nullptr, storm::exceptions::UnexpectedException, "Visitor of jani expression should be of type JaniVisitor.");
    return janiVisitor->visit(*this, data);
}

void ArrayAccessExpression::printToStream(std::ostream& stream) const {
    if (getFirstOperand()->isVariable()) {
        stream << *getFirstOperand();
    } else {
        stream << "(" << *getFirstOperand() << ")";
    }
    stream << "[" << *getSecondOperand() << "]";
}
}  // namespace expressions
}  // namespace storm