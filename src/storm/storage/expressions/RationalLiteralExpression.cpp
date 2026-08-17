#include "storm/storage/expressions/RationalLiteralExpression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/ExpressionVisitor.h"

#include "storm/utility/constants.h"

namespace storm {
namespace expressions {
RationalLiteralExpression::RationalLiteralExpression(ExpressionManager const& manager, double value)
    : BaseExpression(manager, manager.getRationalType()), value(storm::utility::convertNumber<storm::RationalNumber>(value)) {
    // Intentionally left empty.
}

RationalLiteralExpression::RationalLiteralExpression(ExpressionManager const& manager, std::string const& valueAsString)
    : BaseExpression(manager, manager.getRationalType()), value(storm::utility::convertNumber<storm::RationalNumber>(valueAsString)) {
    // Intentionally left empty.
}

RationalLiteralExpression::RationalLiteralExpression(ExpressionManager const& manager, storm::RationalNumber const& value)
    : BaseExpression(manager, manager.getRationalType()), value(value) {
    // Intentionally left empty.
}

double RationalLiteralExpression::evaluateAsDouble(Valuation const*) const {
    return this->getValueAsDouble();
}

bool RationalLiteralExpression::isLiteral() const {
    return true;
}

void RationalLiteralExpression::gatherVariables(std::set<storm::expressions::Variable>&) const {}

std::shared_ptr<BaseExpression const> RationalLiteralExpression::simplify() const {
    return this->shared_from_this();
}

boost::any RationalLiteralExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool RationalLiteralExpression::isRationalLiteralExpression() const {
    return true;
}

double RationalLiteralExpression::getValueAsDouble() const {
    return storm::utility::convertNumber<double>(this->value);
}

storm::RationalNumber RationalLiteralExpression::getValue() const {
    return this->value;
}

void RationalLiteralExpression::printToStream(std::ostream& stream) const {
    stream << this->getValue();
}
}  // namespace expressions
}  // namespace storm
