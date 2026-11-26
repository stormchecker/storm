#include "storm/adapters/AddExpressionAdapter.h"

#include "storm-config.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/dd/Add.h"
#include "storm/dd/Bdd.h"
#include "storm/dd/DdManager.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace adapters {
namespace {
using ExpressionTranslationCache = std::unordered_map<std::shared_ptr<storm::expressions::BaseExpression const>, boost::any>;
}

template<storm::dd::DdType Type, typename ValueType>
AddExpressionAdapter<Type, ValueType>::AddExpressionAdapter(
    std::shared_ptr<storm::dd::DdManager<Type>> ddManager,
    std::shared_ptr<std::map<storm::expressions::Variable, storm::expressions::Variable>> const& variableMapping)
    : ddManager(ddManager), variableMapping(variableMapping) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::translate(storm::expressions::BaseExpression const& expression, boost::any const& data) {
    auto cache = boost::any_cast<ExpressionTranslationCache*>(&data);
    if (cache == nullptr) {
        // Retain support for clients invoking accept() directly without a translation context.
        return expression.accept(*this, data);
    }
    auto self = expression.getSharedPointer();
    auto it = (*cache)->find(self);
    if (it != (*cache)->end()) {
        return it->second;
    }
    auto result = expression.accept(*this, data);
    (*cache)->emplace(std::move(self), result);
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> AddExpressionAdapter<Type, ValueType>::translateExpression(storm::expressions::Expression const& expression) {
    // A fresh cache also handles changes made through the externally owned variable mapping or setValue().
    ExpressionTranslationCache cache;
    auto result = translate(expression.getBaseExpression(), &cache);
    if (expression.hasBooleanType()) {
        return boost::any_cast<storm::dd::Bdd<Type>>(result).template toAdd<ValueType>();
    } else {
        return boost::any_cast<storm::dd::Add<Type, ValueType>>(result);
    }
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> AddExpressionAdapter<Type, ValueType>::translateBooleanExpression(storm::expressions::Expression const& expression) {
    STORM_LOG_THROW(expression.hasBooleanType(), storm::exceptions::InvalidArgumentException, "Expected expression of boolean type.");
    ExpressionTranslationCache cache;
    return boost::any_cast<storm::dd::Bdd<Type>>(translate(expression.getBaseExpression(), &cache));
}

template<storm::dd::DdType Type, typename ValueType>
void AddExpressionAdapter<Type, ValueType>::setValue(storm::expressions::Variable const& variable, ValueType const& value) {
    valueMapping[variable] = value;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) {
    if (expression.hasBooleanType()) {
        storm::dd::Bdd<Type> elseDd = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getElseExpression(), data));
        storm::dd::Bdd<Type> thenDd = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getThenExpression(), data));
        storm::dd::Bdd<Type> conditionDd = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getCondition(), data));
        return conditionDd.ite(thenDd, elseDd);
    } else {
        storm::dd::Add<Type, ValueType> elseDd = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getElseExpression(), data));
        storm::dd::Add<Type, ValueType> thenDd = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getThenExpression(), data));
        storm::dd::Bdd<Type> conditionDd = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getCondition(), data));
        return conditionDd.ite(thenDd, elseDd);
    }
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    storm::dd::Bdd<Type> leftResult = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getFirstOperand(), data));
    storm::dd::Bdd<Type> rightResult = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getSecondOperand(), data));

    storm::dd::Bdd<Type> result;
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::And:
            result = (leftResult && rightResult);
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Or:
            result = (leftResult || rightResult);
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Iff:
            result = (leftResult.iff(rightResult));
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Implies:
            result = (!leftResult || rightResult);
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Xor:
            result = (leftResult.exclusiveOr(rightResult));
            break;
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    storm::dd::Add<Type, ValueType> leftResult = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getFirstOperand(), data));
    storm::dd::Add<Type, ValueType> rightResult = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getSecondOperand(), data));

    storm::dd::Add<Type, ValueType> result;
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Plus:
            result = leftResult + rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Minus:
            result = leftResult - rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Times:
            result = leftResult * rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Divide:
            result = leftResult / rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Max:
            result = leftResult.maximum(rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Min:
            result = leftResult.minimum(rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Power:
            result = leftResult.pow(rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Modulo:
            result = leftResult.mod(rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Logarithm:
            result = leftResult.logxy(rightResult);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Cannot translate expression containing power operator.");
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) {
    storm::dd::Add<Type, ValueType> leftResult = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getFirstOperand(), data));
    storm::dd::Add<Type, ValueType> rightResult = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getSecondOperand(), data));

    storm::dd::Bdd<Type> result;
    switch (expression.getRelationType()) {
        case storm::expressions::RelationType::Equal:
            result = leftResult.equals(rightResult);
            break;
        case storm::expressions::RelationType::NotEqual:
            result = leftResult.notEquals(rightResult);
            break;
        case storm::expressions::RelationType::Less:
            result = leftResult.less(rightResult);
            break;
        case storm::expressions::RelationType::LessOrEqual:
            result = leftResult.lessOrEqual(rightResult);
            break;
        case storm::expressions::RelationType::Greater:
            result = leftResult.greater(rightResult);
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            result = leftResult.greaterOrEqual(rightResult);
            break;
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    auto valueIt = valueMapping.find(expression.getVariable());
    if (valueIt != valueMapping.end()) {
        return ddManager->getConstant(valueIt->second);
    } else {
        auto const& variablePair = variableMapping->find(expression.getVariable());
        STORM_LOG_THROW(variablePair != variableMapping->end(), storm::exceptions::InvalidArgumentException,
                        "Cannot translate the given expression, because it contains the variable '" << expression.getVariableName()
                                                                                                    << "' for which no DD counterpart is known.");
        if (expression.hasBooleanType()) {
            return ddManager->template getIdentity<ValueType>(variablePair->second).toBdd();
        } else {
            return ddManager->template getIdentity<ValueType>(variablePair->second);
        }
    }
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    storm::dd::Bdd<Type> result = boost::any_cast<storm::dd::Bdd<Type>>(translate(*expression.getOperand(), data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryBooleanFunctionExpression::OperatorType::Not:
            result = !result;
            break;
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    storm::dd::Add<Type, ValueType> result = boost::any_cast<storm::dd::Add<Type, ValueType>>(translate(*expression.getOperand(), data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Minus:
            result = -result;
            break;
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Floor:
            result = result.floor();
            break;
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Ceil:
            result = result.ceil();
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Cannot translate expression containing floor/ceil operator.");
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) {
    return expression.getValue() ? ddManager->getBddOne() : ddManager->getBddZero();
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) {
    return ddManager->getConstant(storm::utility::convertNumber<ValueType>(expression.getValue()));
}

template<storm::dd::DdType Type, typename ValueType>
boost::any AddExpressionAdapter<Type, ValueType>::visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) {
    return ddManager->getConstant(storm::utility::convertNumber<ValueType>(expression.getValue()));
}

// Explicitly instantiate the symbolic expression adapter
template class AddExpressionAdapter<storm::dd::DdType::CUDD, double>;
template class AddExpressionAdapter<storm::dd::DdType::Sylvan, double>;
template class AddExpressionAdapter<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class AddExpressionAdapter<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace adapters
}  // namespace storm
