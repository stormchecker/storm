#include <map>
#include <string>
#include <unordered_map>

#include "storm/expressions/Expressions.h"
#include "storm/expressions/SubstitutionVisitor.h"

namespace storm {
namespace expressions {
template<typename MapType>
SubstitutionVisitor<MapType>::SubstitutionVisitor(MapType const& variableToExpressionMapping) : variableToExpressionMapping(variableToExpressionMapping) {
    // Intentionally left empty.
}

template<typename MapType>
Expression SubstitutionVisitor<MapType>::substitute(Expression const& expression) {
    substitutionCache.clear();
    auto result = Expression(boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(expression.getBaseExpression(), boost::none)));
    substitutionCache.clear();
    return result;
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visitExpression(BaseExpression const& expression, boost::any const& data) {
    auto self = expression.getSharedPointer();
    auto it = substitutionCache.find(self);
    if (it != substitutionCache.end()) {
        return it->second;
    }
    auto result = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.accept(*this, data));
    substitutionCache.emplace(std::move(self), result);
    return result;
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(IfThenElseExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> conditionExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getCondition(), data));
    std::shared_ptr<BaseExpression const> thenExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getThenExpression(), data));
    std::shared_ptr<BaseExpression const> elseExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getElseExpression(), data));

    // If the arguments did not change, we simply push the expression itself.
    if (conditionExpression.get() == expression.getCondition().get() && thenExpression.get() == expression.getThenExpression().get() &&
        elseExpression.get() == expression.getElseExpression().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new IfThenElseExpression(expression.getManager(), expression.getType(), conditionExpression, thenExpression, elseExpression)));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getFirstOperand(), data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getSecondOperand(), data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
            expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getFirstOperand(), data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getSecondOperand(), data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
            expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getFirstOperand(), data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getSecondOperand(), data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new BinaryRelationExpression(expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getRelationType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(VariableExpression const& expression, boost::any const&) {
    // If the variable is in the key set of the substitution, we need to replace it.
    auto const& nameExpressionPair = this->variableToExpressionMapping.find(expression.getVariable());
    if (nameExpressionPair != this->variableToExpressionMapping.end()) {
        return nameExpressionPair->second.getBaseExpressionPointer();
    } else {
        return expression.getSharedPointer();
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> operandExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getOperand(), data));

    // If the argument did not change, we simply push the expression itself.
    if (operandExpression.get() == expression.getOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new UnaryBooleanFunctionExpression(expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> operandExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getOperand(), data));

    // If the argument did not change, we simply push the expression itself.
    if (operandExpression.get() == expression.getOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new UnaryNumericalFunctionExpression(expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(PredicateExpression const& expression, boost::any const& data) {
    bool changed = false;
    std::vector<std::shared_ptr<BaseExpression const>> newExpressions;
    for (uint64_t i = 0; i < expression.getArity(); ++i) {
        newExpressions.push_back(boost::any_cast<std::shared_ptr<BaseExpression const>>(visitExpression(*expression.getOperand(i), data)));
        if (!changed && newExpressions.back() != expression.getOperand(i)) {
            changed = true;
        }
    }

    // If the arguments did not change, we simply push the expression itself.
    if (!changed) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new PredicateExpression(expression.getManager(), expression.getType(), newExpressions, expression.getPredicateType())));
    }
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(BooleanLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

template<typename MapType>
boost::any SubstitutionVisitor<MapType>::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

// Explicitly instantiate the class with map and unordered_map.
template class SubstitutionVisitor<std::map<Variable, Expression>>;
template class SubstitutionVisitor<std::unordered_map<Variable, Expression>>;
}  // namespace expressions
}  // namespace storm
