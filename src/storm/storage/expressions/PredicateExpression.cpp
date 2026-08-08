
#include "storm/storage/expressions/PredicateExpression.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/expressions/BooleanLiteralExpression.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
OperatorType toOperatorType(PredicateExpression::PredicateType tp) {
    switch (tp) {
        case PredicateExpression::PredicateType::AtMostOneOf:
            return OperatorType::AtMostOneOf;
        case PredicateExpression::PredicateType::AtLeastOneOf:
            return OperatorType::AtLeastOneOf;
        case PredicateExpression::PredicateType::ExactlyOneOf:
            return OperatorType::ExactlyOneOf;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Predicate type not supported.");
}

PredicateExpression::PredicateExpression(ExpressionManager const& manager, Type const& type, std::vector<std::shared_ptr<BaseExpression const>> const& operands,
                                         PredicateType predicateType)
    : BaseExpression(manager, type), predicate(predicateType), operands(operands) {}

// Override base class methods.
storm::expressions::OperatorType PredicateExpression::getOperator() const {
    return toOperatorType(predicate);
}

bool PredicateExpression::evaluateAsBool(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasBooleanType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as boolean.");
    uint64_t nrTrue = 0;
    for (auto const& operand : operands) {
        if (operand->evaluateAsBool(valuation)) {
            nrTrue++;
        }
    }
    switch (predicate) {
        case PredicateType::ExactlyOneOf:
            return nrTrue == 1;
        case PredicateType::AtMostOneOf:
            return nrTrue <= 1;
        case PredicateType::AtLeastOneOf:
            return nrTrue >= 1;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Predicate type not supported.");
}

std::shared_ptr<BaseExpression const> PredicateExpression::simplify() const {
    std::vector<std::shared_ptr<BaseExpression const>> simplifiedOperands;
    uint64_t trueCount = 0;
    for (auto const& operand : operands) {
        auto res = operand->simplify();
        if (res->isLiteral()) {
            if (res->isTrue()) {
                if (predicate == PredicateType::AtLeastOneOf) {
                    return res;
                } else {
                    STORM_LOG_ASSERT(predicate == PredicateType::AtMostOneOf || predicate == PredicateType::ExactlyOneOf, "Unexpected predicate.");
                    simplifiedOperands.push_back(res);
                }
            } else {
                STORM_LOG_ASSERT(res->isFalse(), "Expected false literal.");
                STORM_LOG_ASSERT(
                    predicate == PredicateType::AtMostOneOf || predicate == PredicateType::AtLeastOneOf || predicate == PredicateType::ExactlyOneOf,
                    "Unexpected predicate.");
                // do nothing, in particular, do not add.
            }
        } else {
            simplifiedOperands.push_back(res);
        }
    }

    if (simplifiedOperands.size() == 0) {
        switch (predicate) {
            case PredicateType::ExactlyOneOf:
                return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), trueCount == 1));
            case PredicateType::AtLeastOneOf:
                return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), trueCount >= 1));
            case PredicateType::AtMostOneOf:
                return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), trueCount <= 1));
        }
    }
    // Return new expression if something changed.
    if (simplifiedOperands.size() != operands.size()) {
        return std::shared_ptr<BaseExpression>(new PredicateExpression(this->getManager(), this->getType(), simplifiedOperands, predicate));
    }
    for (uint64_t i = 0; i < simplifiedOperands.size(); ++i) {
        if (operands[i] != simplifiedOperands[i]) {
            return std::shared_ptr<BaseExpression>(new PredicateExpression(this->getManager(), this->getType(), simplifiedOperands, predicate));
        }
    }
    // All operands remained the same.
    return this->shared_from_this();
}

boost::any PredicateExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool PredicateExpression::isPredicateExpression() const {
    return true;
}

bool PredicateExpression::isFunctionApplication() const {
    return true;
}

bool PredicateExpression::containsVariables() const {
    for (auto const& operand : operands) {
        if (operand->containsVariables()) {
            return true;
        }
    }
    return false;
}

uint_fast64_t PredicateExpression::getArity() const {
    return operands.size();
}

std::shared_ptr<BaseExpression const> PredicateExpression::getOperand(uint_fast64_t operandIndex) const {
    STORM_LOG_ASSERT(operandIndex < this->getArity(), "Invalid operand access.");
    return operands[operandIndex];
}

void PredicateExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    for (auto const& operand : operands) {
        operand->gatherVariables(variables);
    }
}

/*!
 * Retrieves the relation associated with the expression.
 *
 * @return The relation associated with the expression.
 */
PredicateExpression::PredicateType PredicateExpression::getPredicateType() const {
    return predicate;
}

void PredicateExpression::printToStream(std::ostream& stream) const {
    switch (this->getPredicateType()) {
        case PredicateExpression::PredicateType::AtMostOneOf:
            stream << "atMostOneOf(";
            break;
        case PredicateExpression::PredicateType::AtLeastOneOf:
            stream << "atLeastOneOf(";
            break;
        case PredicateExpression::PredicateType::ExactlyOneOf:
            stream << "exactlyOneOf(";
            break;
    }
    if (!operands.empty()) {
        stream << *operands[0];
        for (uint64_t i = 1; i < operands.size(); i++) {
            stream << ", " << *operands[i];
        }
    }
    stream << ")";
}
}  // namespace expressions
}  // namespace storm
