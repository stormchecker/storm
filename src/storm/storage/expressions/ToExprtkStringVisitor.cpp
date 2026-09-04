#include "storm/storage/expressions/ToExprtkStringVisitor.h"

#include "storm/numbers/constants.h"

namespace storm {
namespace expressions {
std::string ToExprtkStringVisitor::toString(Expression const& expression) {
    return toString(expression.getBaseExpressionPointer().get());
}

std::string ToExprtkStringVisitor::toString(BaseExpression const* expression) {
    stream.str("");
    stream.clear();
    expression->accept(*this, boost::none);
    return stream.str();
}

boost::any ToExprtkStringVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    stream << "if(";
    expression.getCondition()->accept(*this, data);
    stream << ",";
    expression.getThenExpression()->accept(*this, data);
    stream << ",";
    expression.getElseExpression()->accept(*this, data);
    stream << ")";
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case BinaryBooleanFunctionExpression::OperatorType::And:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " & ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Or:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " | ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Xor:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " xor ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Implies:
            stream << "(not(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ") | ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Iff:
            expression.getFirstOperand()->accept(*this, data);
            stream << "==";
            expression.getSecondOperand()->accept(*this, data);
            break;
    }
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case BinaryNumericalFunctionExpression::OperatorType::Plus:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "+";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Minus:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "-";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Times:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "*";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Divide:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "/";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Power:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "^";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Modulo:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "%";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Logarithm:
            if (expression.getSecondOperand()->isLiteral()) {
                auto base = expression.getSecondOperand()->evaluateAsRational();
                if (base == storm::numbers::convert<storm::RationalNumber, uint64_t>(2ull)) {
                    stream << "log2(";
                    expression.getFirstOperand()->accept(*this, data);
                    stream << ")";
                    break;
                } else if (base == storm::numbers::convert<storm::RationalNumber, uint64_t>(10ull)) {
                    stream << "log10(";
                    expression.getFirstOperand()->accept(*this, data);
                    stream << ")";
                    break;
                }
            }
            stream << "logn(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ",";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Max:
            stream << "max(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ",";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Min:
            stream << "min(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ",";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
    }
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    switch (expression.getRelationType()) {
        case RelationType::Equal:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "==";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case RelationType::NotEqual:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "!=";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case RelationType::Less:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "<";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case RelationType::LessOrEqual:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "<=";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case RelationType::Greater:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ">";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case RelationType::GreaterOrEqual:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ">=";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
    }
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(VariableExpression const& expression, boost::any const&) {
    stream << "v" + std::to_string(expression.getVariable().getIndex());
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case UnaryBooleanFunctionExpression::OperatorType::Not:
            stream << "not(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
    }
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case UnaryNumericalFunctionExpression::OperatorType::Minus:
            stream << "-(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Floor:
            stream << "floor(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Ceil:
            stream << "ceil(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Cos:
            stream << "cos(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
            break;
        case UnaryNumericalFunctionExpression::OperatorType::Sin:
            stream << "sin(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
            break;
    }
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(BooleanLiteralExpression const& expression, boost::any const&) {
    stream << expression.getValue();
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    stream << expression.getValue();
    return boost::any();
}

boost::any ToExprtkStringVisitor::visit(RationalLiteralExpression const& expression, boost::any const&) {
    stream << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << "(" << expression.getValueAsDouble() << ")";
    return boost::any();
}
}  // namespace expressions
}  // namespace storm
