#include "ReduceNestingVisitor.h"

#include <string>

#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {

ReduceNestingVisitor::ReduceNestingVisitor() {
    // Intentionally left empty.
}

Expression ReduceNestingVisitor::reduceNesting(Expression const& expression) {
    return Expression(boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getBaseExpression().accept(*this, boost::none)));
}

boost::any ReduceNestingVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> conditionExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getCondition()->accept(*this, data));
    std::shared_ptr<BaseExpression const> thenExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getThenExpression()->accept(*this, data));
    std::shared_ptr<BaseExpression const> elseExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getElseExpression()->accept(*this, data));

    // If the arguments did not change, we simply push the expression itself.
    if (conditionExpression.get() == expression.getCondition().get() && thenExpression.get() == expression.getThenExpression().get() &&
        elseExpression.get() == expression.getElseExpression().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new IfThenElseExpression(expression.getManager(), expression.getType(), conditionExpression, thenExpression, elseExpression)));
    }
}

template<typename BinaryFunc>
std::vector<std::shared_ptr<BaseExpression const>> getAllOperands(BinaryFunc const& binaryExpression) {
    auto opType = binaryExpression.getOperatorType();
    std::vector<std::shared_ptr<BaseExpression const>> stack = {binaryExpression.getSharedPointer()};
    std::vector<std::shared_ptr<BaseExpression const>> res;
    while (!stack.empty()) {
        auto f = std::move(stack.back());
        stack.pop_back();

        for (uint64_t opIndex = 0; opIndex < 2; ++opIndex) {
            BinaryFunc const* subexp = dynamic_cast<BinaryFunc const*>(f->getOperand(opIndex).get());
            if (subexp != nullptr && subexp->getOperatorType() == opType) {
                stack.push_back(f->getOperand(opIndex));
            } else {
                res.push_back(f->getOperand(opIndex));
            }
        }
    }
    return res;
}

boost::any ReduceNestingVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    // Check if the operator is commutative and associative
    if (expression.getOperatorType() == BinaryBooleanFunctionExpression::OperatorType::Or ||
        expression.getOperatorType() == BinaryBooleanFunctionExpression::OperatorType::And ||
        expression.getOperatorType() == BinaryBooleanFunctionExpression::OperatorType::Iff) {
        std::vector<std::shared_ptr<BaseExpression const>> operands = getAllOperands<BinaryBooleanFunctionExpression>(expression);

        // Balance the syntax tree if there are enough operands
        if (operands.size() >= 4) {
            for (auto& operand : operands) {
                operand = boost::any_cast<std::shared_ptr<BaseExpression const>>(operand->accept(*this, data));
            }

            auto opIt = operands.begin();
            while (operands.size() > 1) {
                if (opIt == operands.end() || opIt == operands.end() - 1) {
                    opIt = operands.begin();
                }
                *opIt = std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
                    new BinaryBooleanFunctionExpression(expression.getManager(), expression.getType(), *opIt, operands.back(), expression.getOperatorType())));
                operands.pop_back();
                ++opIt;
            }
            return operands.front();
        }
    }

    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
            expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
    }
}

boost::any ReduceNestingVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    // Check if the operator is commutative and associative
    if (expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Plus ||
        expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Times ||
        expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Max ||
        expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Min) {
        std::vector<std::shared_ptr<BaseExpression const>> operands = getAllOperands<BinaryNumericalFunctionExpression>(expression);

        // Balance the syntax tree if there are enough operands
        if (operands.size() >= 4) {
            for (auto& operand : operands) {
                operand = boost::any_cast<std::shared_ptr<BaseExpression const>>(operand->accept(*this, data));
            }

            auto opIt = operands.begin();
            while (operands.size() > 1) {
                if (opIt == operands.end() || opIt == operands.end() - 1) {
                    opIt = operands.begin();
                }
                *opIt = std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
                    expression.getManager(), expression.getType(), *opIt, operands.back(), expression.getOperatorType())));
                operands.pop_back();
                ++opIt;
            }
            return operands.front();
        }
    }

    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
            expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
    }
}

boost::any ReduceNestingVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> firstExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    std::shared_ptr<BaseExpression const> secondExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));

    // If the arguments did not change, we simply push the expression itself.
    if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new BinaryRelationExpression(expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getRelationType())));
    }
}

boost::any ReduceNestingVisitor::visit(VariableExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

boost::any ReduceNestingVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> operandExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getOperand()->accept(*this, data));

    // If the argument did not change, we simply push the expression itself.
    if (operandExpression.get() == expression.getOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new UnaryBooleanFunctionExpression(expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
    }
}

boost::any ReduceNestingVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    std::shared_ptr<BaseExpression const> operandExpression =
        boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getOperand()->accept(*this, data));

    // If the argument did not change, we simply push the expression itself.
    if (operandExpression.get() == expression.getOperand().get()) {
        return expression.getSharedPointer();
    } else {
        return std::const_pointer_cast<BaseExpression const>(std::shared_ptr<BaseExpression>(
            new UnaryNumericalFunctionExpression(expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
    }
}

boost::any ReduceNestingVisitor::visit(BooleanLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

boost::any ReduceNestingVisitor::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

boost::any ReduceNestingVisitor::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return expression.getSharedPointer();
}

}  // namespace expressions
}  // namespace storm
