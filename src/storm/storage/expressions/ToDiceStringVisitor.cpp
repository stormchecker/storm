#include "storm/storage/expressions/ToDiceStringVisitor.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {
ToDiceStringVisitor::ToDiceStringVisitor(uint64_t nrBits) : nrBits(nrBits) {}

std::string ToDiceStringVisitor::toString(Expression const& expression) {
    return toString(expression.getBaseExpressionPointer().get());
}

std::string ToDiceStringVisitor::toString(BaseExpression const* expression) {
    stream.str("");
    stream.clear();
    expression->accept(*this, boost::none);
    return stream.str();
}

boost::any ToDiceStringVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    stream << "if ";
    expression.getCondition()->accept(*this, data);
    stream << " then ";
    expression.getThenExpression()->accept(*this, data);
    stream << " else ";
    expression.getElseExpression()->accept(*this, data);
    stream << "";
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case BinaryBooleanFunctionExpression::OperatorType::And:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " && ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Or:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " || ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Xor:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << " ^ ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Implies:
            stream << "(!(";
            expression.getFirstOperand()->accept(*this, data);
            stream << ") || ";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryBooleanFunctionExpression::OperatorType::Iff:
            expression.getFirstOperand()->accept(*this, data);
            stream << " <=> ";
            expression.getSecondOperand()->accept(*this, data);
            break;
    }
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
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
        case BinaryNumericalFunctionExpression::OperatorType::Divide: {
            STORM_LOG_THROW(expression.getSecondOperand()->isIntegerLiteralExpression(), storm::exceptions::NotSupportedException,
                            "Dice does not support modulo with nonconst rhs");
            uint64_t denominator = expression.getSecondOperand()->evaluateAsInt();
            int shifts = 0;
            while (denominator % 2 == 0) {
                denominator = denominator >> 1;
                shifts++;
            }
            denominator = denominator >> 1;
            STORM_LOG_THROW(denominator <= 0, storm::exceptions::NotSupportedException, "Dice does not support division with non-powers of two");
            if (shifts > 0) {
                stream << "(";
                expression.getFirstOperand()->accept(*this, data);
                stream << " >> " << shifts;
                stream << ")";
            } else {
                expression.getFirstOperand()->accept(*this, data);
            }

        } break;
        case BinaryNumericalFunctionExpression::OperatorType::Power:
            stream << "(";
            expression.getFirstOperand()->accept(*this, data);
            stream << "^";
            expression.getSecondOperand()->accept(*this, data);
            stream << ")";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Modulo:
            STORM_LOG_THROW(expression.getSecondOperand()->isIntegerLiteralExpression(), storm::exceptions::NotSupportedException,
                            "Dice does not support modulo with nonconst rhs");
            STORM_LOG_THROW(expression.getSecondOperand()->evaluateAsInt() == 2, storm::exceptions::NotSupportedException,
                            "Dice does not support modulo with rhs != 2");

            stream << "( nth_bit(int(" << nrBits << "," << nrBits - 1 << "), ";
            expression.getFirstOperand()->accept(*this, data);
            stream << "))";
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Logarithm:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Dice translation not supported for log expressions.");
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

boost::any ToDiceStringVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
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

boost::any ToDiceStringVisitor::visit(VariableExpression const& expression, boost::any const&) {
    stream << expression.getVariable().getName();
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    switch (expression.getOperatorType()) {
        case UnaryBooleanFunctionExpression::OperatorType::Not:
            stream << "!(";
            expression.getOperand()->accept(*this, data);
            stream << ")";
    }
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
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
        case UnaryNumericalFunctionExpression::OperatorType::Sin:
        case UnaryNumericalFunctionExpression::OperatorType::Cos:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Dice does not support sin/cos functions.");
            break;
    }
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(PredicateExpression const& expression, boost::any const& data) {
    auto const& pdt = expression.getPredicateType();
    STORM_LOG_ASSERT(pdt == PredicateExpression::PredicateType::ExactlyOneOf || pdt == PredicateExpression::PredicateType::AtLeastOneOf ||
                         pdt == PredicateExpression::PredicateType::AtMostOneOf,
                     "Only some predicate types are supported.");
    stream << "(";
    if (pdt == PredicateExpression::PredicateType::ExactlyOneOf || pdt == PredicateExpression::PredicateType::AtMostOneOf) {
        stream << "(true ";
        for (uint64_t operandi = 0; operandi < expression.getArity(); ++operandi) {
            for (uint64_t operandj = operandi + 1; operandj < expression.getArity(); ++operandj) {
                stream << "&& !(";
                expression.getOperand(operandi)->accept(*this, data);
                stream << " && ";
                expression.getOperand(operandj)->accept(*this, data);
                stream << ")";
            }
        }
        stream << ")";
    }
    if (pdt == PredicateExpression::PredicateType::ExactlyOneOf) {
        stream << " && ";
    }
    if (pdt == PredicateExpression::PredicateType::ExactlyOneOf || pdt == PredicateExpression::PredicateType::AtLeastOneOf) {
        stream << "( false";
        for (uint64_t operandj = 0; operandj < expression.getArity(); ++operandj) {
            stream << "|| ";
            expression.getOperand(operandj)->accept(*this, data);
        }
        stream << ")";
    }
    stream << ")";
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(BooleanLiteralExpression const& expression, boost::any const&) {
    stream << (expression.getValue() ? " true " : " false ");
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    stream << "int(" << nrBits << "," << expression.getValue() << ")";
    return boost::any();
}

boost::any ToDiceStringVisitor::visit(RationalLiteralExpression const& expression, boost::any const&) {
    stream << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << "(" << expression.getValueAsDouble() << ")";
    return boost::any();
}
}  // namespace expressions
}  // namespace storm
