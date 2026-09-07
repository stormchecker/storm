#include "SyntacticalEqualityCheckVisitor.h"

#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {

bool SyntacticalEqualityCheckVisitor::isSyntacticallyEqual(storm::expressions::Expression const& expression1,
                                                           storm::expressions::Expression const& expression2) {
    return boost::any_cast<bool>(expression1.accept(*this, std::ref(expression2.getBaseExpression())));
}

boost::any SyntacticalEqualityCheckVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isIfThenElseExpression()) {
        IfThenElseExpression const& otherExpression = otherBaseExpression.asIfThenElseExpression();

        bool result = boost::any_cast<bool>(expression.getCondition()->accept(*this, std::ref(*otherExpression.getCondition())));
        if (result) {
            result = boost::any_cast<bool>(expression.getThenExpression()->accept(*this, std::ref(*otherExpression.getThenExpression())));
        }
        if (result) {
            result = boost::any_cast<bool>(expression.getElseExpression()->accept(*this, std::ref(*otherExpression.getElseExpression())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isBinaryBooleanFunctionExpression()) {
        BinaryBooleanFunctionExpression const& otherExpression = otherBaseExpression.asBinaryBooleanFunctionExpression();

        bool result = expression.getOperatorType() == otherExpression.getOperatorType();
        if (result) {
            result = boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, std::ref(*otherExpression.getFirstOperand())));
        }
        if (result) {
            result = boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, std::ref(*otherExpression.getSecondOperand())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isBinaryNumericalFunctionExpression()) {
        BinaryNumericalFunctionExpression const& otherExpression = otherBaseExpression.asBinaryNumericalFunctionExpression();

        bool result = expression.getOperatorType() == otherExpression.getOperatorType();
        if (result) {
            result = boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, std::ref(*otherExpression.getFirstOperand())));
        }
        if (result) {
            result = boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, std::ref(*otherExpression.getSecondOperand())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isBinaryRelationExpression()) {
        BinaryRelationExpression const& otherExpression = otherBaseExpression.asBinaryRelationExpression();

        bool result = expression.getRelationType() == otherExpression.getRelationType();
        if (result) {
            result = boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, std::ref(*otherExpression.getFirstOperand())));
        }
        if (result) {
            result = boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, std::ref(*otherExpression.getSecondOperand())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(VariableExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isVariableExpression()) {
        VariableExpression const& otherExpression = otherBaseExpression.asVariableExpression();
        return expression.getVariable() == otherExpression.getVariable();
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isUnaryBooleanFunctionExpression()) {
        UnaryBooleanFunctionExpression const& otherExpression = otherBaseExpression.asUnaryBooleanFunctionExpression();

        bool result = expression.getOperatorType() == otherExpression.getOperatorType();
        if (result) {
            result = boost::any_cast<bool>(expression.getOperand()->accept(*this, std::ref(*otherExpression.getOperand())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isUnaryNumericalFunctionExpression()) {
        UnaryNumericalFunctionExpression const& otherExpression = otherBaseExpression.asUnaryNumericalFunctionExpression();

        bool result = expression.getOperatorType() == otherExpression.getOperatorType();
        if (result) {
            result = boost::any_cast<bool>(expression.getOperand()->accept(*this, std::ref(*otherExpression.getOperand())));
        }
        return result;
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(BooleanLiteralExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isBooleanLiteralExpression()) {
        BooleanLiteralExpression const& otherExpression = otherBaseExpression.asBooleanLiteralExpression();
        return expression.getValue() == otherExpression.getValue();
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(IntegerLiteralExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isIntegerLiteralExpression()) {
        IntegerLiteralExpression const& otherExpression = otherBaseExpression.asIntegerLiteralExpression();
        return expression.getValue() == otherExpression.getValue();
    } else {
        return false;
    }
}

boost::any SyntacticalEqualityCheckVisitor::visit(RationalLiteralExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    if (otherBaseExpression.isRationalLiteralExpression()) {
        RationalLiteralExpression const& otherExpression = otherBaseExpression.asRationalLiteralExpression();
        return expression.getValue() == otherExpression.getValue();
    } else {
        return false;
    }
}

}  // namespace expressions
}  // namespace storm
