#include "storm/storage/expressions/ToRationalFunctionVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

template<typename RationalFunctionType>
ToRationalFunctionVisitor<RationalFunctionType>::ToRationalFunctionVisitor(ExpressionEvaluatorBase<RationalFunctionType> const& evaluator)
    : ExpressionVisitor(), cache(new storm::RawPolynomialCache()), evaluator(evaluator) {
    // Intentionally left empty.
}

template<typename RationalFunctionType>
RationalFunctionType ToRationalFunctionVisitor<RationalFunctionType>::toRationalFunction(Expression const& expression) {
    return boost::any_cast<RationalFunctionType>(expression.accept(*this, boost::none));
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(IfThenElseExpression const& expression, boost::any const& data) {
    bool conditionValue = evaluator.asBool(expression.getCondition());
    if (conditionValue) {
        return expression.getThenExpression()->accept(*this, data);
    } else {
        return expression.getElseExpression()->accept(*this, data);
    }
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(BinaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational function.");
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    RationalFunctionType firstOperandAsRationalFunction = boost::any_cast<RationalFunctionType>(expression.getFirstOperand()->accept(*this, data));
    RationalFunctionType secondOperandAsRationalFunction = boost::any_cast<RationalFunctionType>(expression.getSecondOperand()->accept(*this, data));
    switch (expression.getOperatorType()) {
        case BinaryNumericalFunctionExpression::OperatorType::Plus:
            return firstOperandAsRationalFunction + secondOperandAsRationalFunction;
        case BinaryNumericalFunctionExpression::OperatorType::Minus:
            return firstOperandAsRationalFunction - secondOperandAsRationalFunction;
        case BinaryNumericalFunctionExpression::OperatorType::Times:
            return firstOperandAsRationalFunction * secondOperandAsRationalFunction;
        case BinaryNumericalFunctionExpression::OperatorType::Divide:
            return firstOperandAsRationalFunction / secondOperandAsRationalFunction;
        case BinaryNumericalFunctionExpression::OperatorType::Power: {
            STORM_LOG_THROW(storm::utility::isInteger(secondOperandAsRationalFunction), storm::exceptions::InvalidArgumentException,
                            "Exponent of power operator must be an integer but is " << secondOperandAsRationalFunction << ".");
            auto exponentAsInteger = storm::utility::convertNumber<carl::sint>(secondOperandAsRationalFunction);
            return storm::utility::pow(firstOperandAsRationalFunction, exponentAsInteger);
        }
        default:
            STORM_LOG_ASSERT(false, "Illegal operator type " << expression.getOperator() << " in expression" << expression << ".");
    }

    // Return a dummy. This point must, however, never be reached.
    return boost::any();
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(BinaryRelationExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational function.");
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(VariableExpression const& expression, boost::any const&) {
    auto valueIt = valueMapping.find(expression.getVariable());
    if (valueIt != valueMapping.end()) {
        return valueIt->second;
    }

    auto variablePair = variableToVariableMap.find(expression.getVariable());
    if (variablePair != variableToVariableMap.end()) {
        return convertVariableToPolynomial(variablePair->second);
    } else {
        storm::RationalFunctionVariable carlVariable = storm::createRFVariable(expression.getVariableName());
        variableToVariableMap.emplace(expression.getVariable(), carlVariable);
        return convertVariableToPolynomial(carlVariable);
    }
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(UnaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational function.");
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    RationalFunctionType operandAsRationalFunction = boost::any_cast<RationalFunctionType>(expression.getOperand()->accept(*this, data));
    switch (expression.getOperatorType()) {
        case UnaryNumericalFunctionExpression::OperatorType::Minus:
            return -operandAsRationalFunction;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational function.");
    }
    return boost::any();
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(BooleanLiteralExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression cannot be translated into a rational function.");
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return RationalFunctionType(storm::utility::convertNumber<storm::RationalFunction>(expression.getValue()));
}

template<typename RationalFunctionType>
boost::any ToRationalFunctionVisitor<RationalFunctionType>::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return storm::utility::convertNumber<storm::RationalFunction>(expression.getValue());
}

template<typename RationalFunctionType>
void ToRationalFunctionVisitor<RationalFunctionType>::setMapping(storm::expressions::Variable const& variable, RationalFunctionType const& value) {
    valueMapping[variable] = value;
}

template class ToRationalFunctionVisitor<storm::RationalFunction>;
}  // namespace expressions
}  // namespace storm
