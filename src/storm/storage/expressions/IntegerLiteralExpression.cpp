#include "storm/storage/expressions/IntegerLiteralExpression.h"

#include "ExpressionVisitor.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace expressions {
IntegerLiteralExpression::IntegerLiteralExpression(ExpressionManager const& manager, int_fast64_t value)
    : BaseExpression(manager, manager.getIntegerType()), value(value) {
    // Intentionally left empty.
}

int_fast64_t IntegerLiteralExpression::evaluateAsInt(Valuation const*) const {
    return this->getValue();
}

double IntegerLiteralExpression::evaluateAsDouble(Valuation const* valuation) const {
    return static_cast<double>(this->evaluateAsInt(valuation));
}

bool IntegerLiteralExpression::isLiteral() const {
    return true;
}

void IntegerLiteralExpression::gatherVariables(std::set<storm::expressions::Variable>&) const {}

std::shared_ptr<BaseExpression const> IntegerLiteralExpression::simplify() const {
    return this->shared_from_this();
}

boost::any IntegerLiteralExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool IntegerLiteralExpression::isIntegerLiteralExpression() const {
    return true;
}

int_fast64_t IntegerLiteralExpression::getValue() const {
    return this->value;
}

void IntegerLiteralExpression::printToStream(std::ostream& stream) const {
    stream << this->getValue();
}
}  // namespace expressions
}  // namespace storm
