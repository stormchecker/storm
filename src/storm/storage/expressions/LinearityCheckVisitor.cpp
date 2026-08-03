#include "storm/storage/expressions/LinearityCheckVisitor.h"

#include "storm/storage/expressions/Expressions.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
LinearityCheckVisitor::LinearityCheckVisitor() {
    // Intentionally left empty.
}

bool LinearityCheckVisitor::check(Expression const& expression, bool booleanIsLinear) {
    LinearityStatus result = boost::any_cast<LinearityStatus>(expression.getBaseExpression().accept(*this, booleanIsLinear));
    return result == LinearityStatus::LinearWithoutVariables || result == LinearityStatus::LinearContainsVariables;
}

boost::any LinearityCheckVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    bool booleanIsLinear = boost::any_cast<bool>(data);

    if (booleanIsLinear) {
        auto conditionResult = boost::any_cast<LinearityStatus>(expression.getCondition()->accept(*this, booleanIsLinear));
        auto thenResult = boost::any_cast<LinearityStatus>(expression.getThenExpression()->accept(*this, booleanIsLinear));
        auto elseResult = boost::any_cast<LinearityStatus>(expression.getElseExpression()->accept(*this, booleanIsLinear));

        if (conditionResult != LinearityStatus::NonLinear && thenResult != LinearityStatus::NonLinear && elseResult != LinearityStatus::NonLinear) {
            return LinearityStatus::LinearContainsVariables;
        } else {
            return LinearityStatus::NonLinear;
        }
    } else {
        return LinearityStatus::NonLinear;
    }
}

boost::any LinearityCheckVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    bool booleanIsLinear = boost::any_cast<bool>(data);

    if (booleanIsLinear) {
        auto leftResult = boost::any_cast<LinearityStatus>(expression.getFirstOperand()->accept(*this, booleanIsLinear));
        auto rightResult = boost::any_cast<LinearityStatus>(expression.getSecondOperand()->accept(*this, booleanIsLinear));

        if (leftResult != LinearityStatus::NonLinear && rightResult != LinearityStatus::NonLinear) {
            return LinearityStatus::LinearContainsVariables;
        } else {
            return LinearityStatus::NonLinear;
        }
    } else {
        return LinearityStatus::NonLinear;
    }
}

boost::any LinearityCheckVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    LinearityStatus leftResult = boost::any_cast<LinearityStatus>(expression.getFirstOperand()->accept(*this, data));
    if (leftResult == LinearityStatus::NonLinear) {
        return LinearityStatus::NonLinear;
    }

    LinearityStatus rightResult = boost::any_cast<LinearityStatus>(expression.getSecondOperand()->accept(*this, data));
    if (rightResult == LinearityStatus::NonLinear) {
        return LinearityStatus::NonLinear;
    }

    switch (expression.getOperatorType()) {
        case BinaryNumericalFunctionExpression::OperatorType::Plus:
        case BinaryNumericalFunctionExpression::OperatorType::Minus:
            return (leftResult == LinearityStatus::LinearContainsVariables || rightResult == LinearityStatus::LinearContainsVariables
                        ? LinearityStatus::LinearContainsVariables
                        : LinearityStatus::LinearWithoutVariables);
        case BinaryNumericalFunctionExpression::OperatorType::Times:
        case BinaryNumericalFunctionExpression::OperatorType::Divide:
            if (leftResult == LinearityStatus::LinearContainsVariables && rightResult == LinearityStatus::LinearContainsVariables) {
                return LinearityStatus::NonLinear;
            }

            return (leftResult == LinearityStatus::LinearContainsVariables || rightResult == LinearityStatus::LinearContainsVariables
                        ? LinearityStatus::LinearContainsVariables
                        : LinearityStatus::LinearWithoutVariables);
        case BinaryNumericalFunctionExpression::OperatorType::Min:
            return LinearityStatus::NonLinear;
        case BinaryNumericalFunctionExpression::OperatorType::Max:
            return LinearityStatus::NonLinear;
        case BinaryNumericalFunctionExpression::OperatorType::Power:
            return LinearityStatus::NonLinear;
        case BinaryNumericalFunctionExpression::OperatorType::Modulo:
            return LinearityStatus::NonLinear;
        case BinaryNumericalFunctionExpression::OperatorType::Logarithm:
            return LinearityStatus::NonLinear;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Illegal binary numerical expression operator.");
}

boost::any LinearityCheckVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    bool booleanIsLinear = boost::any_cast<bool>(data);

    if (booleanIsLinear) {
        auto leftResult = boost::any_cast<LinearityStatus>(expression.getFirstOperand()->accept(*this, booleanIsLinear));
        auto rightResult = boost::any_cast<LinearityStatus>(expression.getSecondOperand()->accept(*this, booleanIsLinear));

        if (leftResult != LinearityStatus::NonLinear && rightResult != LinearityStatus::NonLinear) {
            return LinearityStatus::LinearContainsVariables;
        } else {
            return LinearityStatus::NonLinear;
        }
    } else {
        return LinearityStatus::NonLinear;
    }
}

boost::any LinearityCheckVisitor::visit(VariableExpression const&, boost::any const&) {
    return LinearityStatus::LinearContainsVariables;
}

boost::any LinearityCheckVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    bool booleanIsLinear = boost::any_cast<bool>(data);

    if (booleanIsLinear) {
        return boost::any_cast<LinearityStatus>(expression.getOperand()->accept(*this, booleanIsLinear));
    } else {
        return LinearityStatus::NonLinear;
    }
}

boost::any LinearityCheckVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case UnaryNumericalFunctionExpression::OperatorType::Minus:
            return expression.getOperand()->accept(*this, data);
        case UnaryNumericalFunctionExpression::OperatorType::Floor:
        case UnaryNumericalFunctionExpression::OperatorType::Ceil:
        case UnaryNumericalFunctionExpression::OperatorType::Cos:
        case UnaryNumericalFunctionExpression::OperatorType::Sin:
            return LinearityStatus::NonLinear;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Illegal unary numerical expression operator.");
}

boost::any LinearityCheckVisitor::visit(BooleanLiteralExpression const&, boost::any const& data) {
    bool booleanIsLinear = boost::any_cast<bool>(data);

    if (booleanIsLinear) {
        return LinearityStatus::LinearWithoutVariables;
    } else {
        return LinearityStatus::NonLinear;
    }
}

boost::any LinearityCheckVisitor::visit(IntegerLiteralExpression const&, boost::any const&) {
    return LinearityStatus::LinearWithoutVariables;
}

boost::any LinearityCheckVisitor::visit(RationalLiteralExpression const&, boost::any const&) {
    return LinearityStatus::LinearWithoutVariables;
}
}  // namespace expressions
}  // namespace storm
