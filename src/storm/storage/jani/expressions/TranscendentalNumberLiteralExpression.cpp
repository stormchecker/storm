#include "storm/exceptions/UnexpectedException.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/expressions/TranscendentalNumberLiteralExpression.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

#include "storm/utility/constants.h"

namespace storm {
namespace expressions {
TranscendentalNumberLiteralExpression::TranscendentalNumberLiteralExpression(ExpressionManager const& manager, TranscendentalNumber const& value)
    : BaseExpression(manager, manager.getTranscendentalNumberType()), value(value) {
    // Intentionally left empty.
}

double TranscendentalNumberLiteralExpression::evaluateAsDouble(Valuation const*) const {
    switch (value) {
        case TranscendentalNumber::PI:
            return M_PI;
        case TranscendentalNumber::E:
            return std::exp(1.0);
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected constant value.");
            break;
    }
}

bool TranscendentalNumberLiteralExpression::isLiteral() const {
    return true;
}

void TranscendentalNumberLiteralExpression::gatherVariables(std::set<storm::expressions::Variable>&) const {
    // A constant value is not supposed to have any variable
}

std::shared_ptr<BaseExpression const> TranscendentalNumberLiteralExpression::simplify() const {
    // No further simplification for constant values
    return this->shared_from_this();
}

boost::any TranscendentalNumberLiteralExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    auto janiVisitor = dynamic_cast<JaniExpressionVisitor*>(&visitor);
    STORM_LOG_ASSERT(janiVisitor != nullptr, "Visitor of jani expression should be of type JaniVisitor.");
    STORM_LOG_THROW(janiVisitor != nullptr, storm::exceptions::UnexpectedException, "Visitor of jani expression should be of type JaniVisitor.");
    return janiVisitor->visit(*this, data);
}

TranscendentalNumberLiteralExpression::TranscendentalNumber const& TranscendentalNumberLiteralExpression::getTranscendentalNumber() const {
    return value;
}

std::string TranscendentalNumberLiteralExpression::asString() const {
    switch (value) {
        case TranscendentalNumber::PI:
            return "π";
        case TranscendentalNumber::E:
            return "e";
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected constant value.");
    }
    return "";
}

void TranscendentalNumberLiteralExpression::printToStream(std::ostream& stream) const {
    stream << asString();
}
}  // namespace expressions
}  // namespace storm
