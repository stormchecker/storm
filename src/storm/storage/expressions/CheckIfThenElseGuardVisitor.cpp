#include "CheckIfThenElseGuardVisitor.h"

#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {

CheckIfThenElseGuardVisitor::CheckIfThenElseGuardVisitor(std::set<storm::expressions::Variable> const& variables) : variables(variables) {
    // Intentionally left empty.
}

bool CheckIfThenElseGuardVisitor::check(storm::expressions::Expression const& expression) {
    return boost::any_cast<bool>(expression.accept(*this, boost::none));
}

boost::any CheckIfThenElseGuardVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    // check whether the 'if' condition depends on one of the variables
    if (expression.getCondition()->toExpression().containsVariable(variables)) {
        return true;
    } else {
        // recurse
        return boost::any_cast<bool>(expression.getThenExpression()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getElseExpression()->accept(*this, data));
    }
}

boost::any CheckIfThenElseGuardVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
           boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
}

boost::any CheckIfThenElseGuardVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
           boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
}

boost::any CheckIfThenElseGuardVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
           boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
}

boost::any CheckIfThenElseGuardVisitor::visit(VariableExpression const&, boost::any const&) {
    return false;
}

boost::any CheckIfThenElseGuardVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    return expression.getOperand()->accept(*this, data);
}

boost::any CheckIfThenElseGuardVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    return expression.getOperand()->accept(*this, data);
}

boost::any CheckIfThenElseGuardVisitor::visit(BooleanLiteralExpression const&, boost::any const&) {
    return false;
}

boost::any CheckIfThenElseGuardVisitor::visit(IntegerLiteralExpression const&, boost::any const&) {
    return false;
}

boost::any CheckIfThenElseGuardVisitor::visit(RationalLiteralExpression const&, boost::any const&) {
    return false;
}

}  // namespace expressions
}  // namespace storm
