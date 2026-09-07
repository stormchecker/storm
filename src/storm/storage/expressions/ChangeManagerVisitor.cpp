#include "ChangeManagerVisitor.h"

#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {

ChangeManagerVisitor::ChangeManagerVisitor(ExpressionManager const& manager) : manager(manager) {
    // Intentionally left empty.
}

Expression ChangeManagerVisitor::changeManager(storm::expressions::Expression const& expression) {
    return Expression(boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.accept(*this, boost::none)));
}

boost::any ChangeManagerVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    auto newCondition = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getCondition()->accept(*this, data));
    auto newThen = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getThenExpression()->accept(*this, data));
    auto newElse = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getElseExpression()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(new IfThenElseExpression(manager, expression.getType(), newCondition, newThen, newElse));
}

boost::any ChangeManagerVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto newFirstOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    auto newSecondOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(
        new BinaryBooleanFunctionExpression(manager, expression.getType(), newFirstOperand, newSecondOperand, expression.getOperatorType()));
}

boost::any ChangeManagerVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto newFirstOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    auto newSecondOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(
        new BinaryNumericalFunctionExpression(manager, expression.getType(), newFirstOperand, newSecondOperand, expression.getOperatorType()));
}

boost::any ChangeManagerVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    auto newFirstOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getFirstOperand()->accept(*this, data));
    auto newSecondOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getSecondOperand()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(
        new BinaryRelationExpression(manager, expression.getType(), newFirstOperand, newSecondOperand, expression.getRelationType()));
}

boost::any ChangeManagerVisitor::visit(VariableExpression const& expression, boost::any const&) {
    return std::shared_ptr<BaseExpression const>(new VariableExpression(manager.getVariable(expression.getVariableName())));
}

boost::any ChangeManagerVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto newOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getOperand()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(new UnaryBooleanFunctionExpression(manager, expression.getType(), newOperand, expression.getOperatorType()));
}

boost::any ChangeManagerVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto newOperand = boost::any_cast<std::shared_ptr<BaseExpression const>>(expression.getOperand()->accept(*this, data));
    return std::shared_ptr<BaseExpression const>(new UnaryNumericalFunctionExpression(manager, expression.getType(), newOperand, expression.getOperatorType()));
}

boost::any ChangeManagerVisitor::visit(BooleanLiteralExpression const& expression, boost::any const&) {
    return std::shared_ptr<BaseExpression const>(new BooleanLiteralExpression(manager, expression.getValue()));
}

boost::any ChangeManagerVisitor::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return std::shared_ptr<BaseExpression const>(new IntegerLiteralExpression(manager, expression.getValue()));
}

boost::any ChangeManagerVisitor::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return std::shared_ptr<BaseExpression const>(new RationalLiteralExpression(manager, expression.getValue()));
}

}  // namespace expressions
}  // namespace storm
