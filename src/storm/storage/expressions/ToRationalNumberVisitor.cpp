#include "storm/storage/expressions/ToRationalNumberVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/constants.h"

namespace storm {
namespace expressions {
template<typename RationalNumberType>
ToRationalNumberVisitor<RationalNumberType>::ToRationalNumberVisitor() : ExpressionVisitor() {
    // Intentionally left empty.
}

template<typename RationalNumberType>
ToRationalNumberVisitor<RationalNumberType>::ToRationalNumberVisitor(ExpressionEvaluatorBase<RationalNumberType> const& evaluator)
    : ExpressionVisitor(), evaluator(evaluator) {
    // Intentionally left empty.
}

template<typename RationalNumberType>
RationalNumberType ToRationalNumberVisitor<RationalNumberType>::toRationalNumber(Expression const& expression) {
    return boost::any_cast<RationalNumberType>(expression.accept(*this, boost::none));
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(IfThenElseExpression const& expression, boost::any const& data) {
    bool conditionValue;
    if (evaluator) {
        conditionValue = evaluator->asBool(expression.getCondition());
    } else {
        // no evaluator, fall back to evaluateBool
        conditionValue = expression.getCondition()->evaluateAsBool();
    }
    if (conditionValue) {
        return expression.getThenExpression()->accept(*this, data);
    } else {
        return expression.getElseExpression()->accept(*this, data);
    }
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(BinaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational number.");
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    RationalNumberType firstOperandAsRationalNumber = boost::any_cast<RationalNumberType>(expression.getFirstOperand()->accept(*this, data));
    RationalNumberType secondOperandAsRationalNumber = boost::any_cast<RationalNumberType>(expression.getSecondOperand()->accept(*this, data));
    RationalNumberType result;
    switch (expression.getOperatorType()) {
        case BinaryNumericalFunctionExpression::OperatorType::Plus:
            result = firstOperandAsRationalNumber + secondOperandAsRationalNumber;
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Minus:
            result = firstOperandAsRationalNumber - secondOperandAsRationalNumber;
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Times:
            result = firstOperandAsRationalNumber * secondOperandAsRationalNumber;
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Divide:
            result = firstOperandAsRationalNumber / secondOperandAsRationalNumber;
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Min:
            result = std::min(firstOperandAsRationalNumber, secondOperandAsRationalNumber);
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Max:
            result = std::max(firstOperandAsRationalNumber, secondOperandAsRationalNumber);
            return result;
        case BinaryNumericalFunctionExpression::OperatorType::Power: {
            STORM_LOG_THROW(storm::utility::isInteger(secondOperandAsRationalNumber), storm::exceptions::InvalidArgumentException,
                            "Exponent of power operator must be an integer.");
            auto exponentAsInteger = storm::utility::convertNumber<int_fast64_t>(secondOperandAsRationalNumber);
            result = storm::utility::pow(firstOperandAsRationalNumber, exponentAsInteger);
            return result;
        }
        default:
            STORM_LOG_ASSERT(false, "Illegal operator type.");
    }

    // Return a dummy. This point must, however, never be reached.
    STORM_LOG_ASSERT(false, "Illegal operator type.");
    return boost::any();
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(BinaryRelationExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational number.");
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(VariableExpression const& expression, boost::any const&) {
    return valueMapping.at(expression.getVariable());
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(UnaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational number.");
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    RationalNumberType operandAsRationalNumber = boost::any_cast<RationalNumberType>(expression.getOperand()->accept(*this, data));
    RationalNumberType result;
    switch (expression.getOperatorType()) {
        case UnaryNumericalFunctionExpression::OperatorType::Minus:
            result = -operandAsRationalNumber;
            return result;
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Floor:
            result = storm::utility::floor(operandAsRationalNumber);
            return result;
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Ceil:
            result = storm::utility::ceil(operandAsRationalNumber);
            return result;
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Sin:
            result = storm::utility::sin(operandAsRationalNumber);
            return result;
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Cos:
            result = storm::utility::cos(operandAsRationalNumber);
            return result;
            break;
    }
    // Dummy return.
    return result;
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(BooleanLiteralExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational number.");
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return RationalNumberType(carl::rationalize<storm::RationalNumber>(static_cast<carl::sint>(expression.getValue())));
}

template<typename RationalNumberType>
boost::any ToRationalNumberVisitor<RationalNumberType>::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return expression.getValue();
}

template<typename RationalNumberType>
void ToRationalNumberVisitor<RationalNumberType>::setMapping(storm::expressions::Variable const& variable, RationalNumberType const& value) {
    valueMapping[variable] = value;
}

template class ToRationalNumberVisitor<storm::RationalNumber>;

}  // namespace expressions
}  // namespace storm
