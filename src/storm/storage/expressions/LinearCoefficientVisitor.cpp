#include "LinearCoefficientVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
LinearCoefficientVisitor::VariableCoefficients::VariableCoefficients(double constantPart) : variableToCoefficientMapping(), constantPart(constantPart) {
    // Intentionally left empty.
}

LinearCoefficientVisitor::VariableCoefficients& LinearCoefficientVisitor::VariableCoefficients::operator+=(VariableCoefficients&& other) {
    for (auto const& otherVariableCoefficientPair : other.variableToCoefficientMapping) {
        this->variableToCoefficientMapping[otherVariableCoefficientPair.first] += otherVariableCoefficientPair.second;
    }
    constantPart += other.constantPart;
    return *this;
}

LinearCoefficientVisitor::VariableCoefficients& LinearCoefficientVisitor::VariableCoefficients::operator-=(VariableCoefficients&& other) {
    for (auto const& otherVariableCoefficientPair : other.variableToCoefficientMapping) {
        this->variableToCoefficientMapping[otherVariableCoefficientPair.first] -= otherVariableCoefficientPair.second;
    }
    constantPart -= other.constantPart;
    return *this;
}

LinearCoefficientVisitor::VariableCoefficients& LinearCoefficientVisitor::VariableCoefficients::operator*=(VariableCoefficients&& other) {
    STORM_LOG_THROW(variableToCoefficientMapping.size() == 0 || other.variableToCoefficientMapping.size() == 0, storm::exceptions::InvalidArgumentException,
                    "Expression is non-linear.");
    if (other.variableToCoefficientMapping.size() > 0) {
        variableToCoefficientMapping = std::move(other.variableToCoefficientMapping);
        std::swap(constantPart, other.constantPart);
    }
    for (auto& variableCoefficientPair : this->variableToCoefficientMapping) {
        variableCoefficientPair.second *= other.constantPart;
    }
    constantPart *= other.constantPart;
    return *this;
}

LinearCoefficientVisitor::VariableCoefficients& LinearCoefficientVisitor::VariableCoefficients::operator/=(VariableCoefficients&& other) {
    STORM_LOG_THROW(other.variableToCoefficientMapping.size() == 0, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
    for (auto& variableCoefficientPair : this->variableToCoefficientMapping) {
        variableCoefficientPair.second /= other.constantPart;
    }
    constantPart /= other.constantPart;
    return *this;
}

void LinearCoefficientVisitor::VariableCoefficients::negate() {
    for (auto& variableCoefficientPair : variableToCoefficientMapping) {
        variableCoefficientPair.second = -variableCoefficientPair.second;
    }
    constantPart = -constantPart;
}

void LinearCoefficientVisitor::VariableCoefficients::setCoefficient(storm::expressions::Variable const& variable, double coefficient) {
    variableToCoefficientMapping[variable] = coefficient;
}

double LinearCoefficientVisitor::VariableCoefficients::getCoefficient(storm::expressions::Variable const& variable) {
    return variableToCoefficientMapping[variable];
}

size_t LinearCoefficientVisitor::VariableCoefficients::size() const {
    return variableToCoefficientMapping.size();
}

double LinearCoefficientVisitor::VariableCoefficients::getConstantPart() const {
    return this->constantPart;
}

void LinearCoefficientVisitor::VariableCoefficients::separateVariablesFromConstantPart(VariableCoefficients& rhs) {
    for (auto const& rhsVariableCoefficientPair : rhs.variableToCoefficientMapping) {
        this->variableToCoefficientMapping[rhsVariableCoefficientPair.first] -= rhsVariableCoefficientPair.second;
    }
    rhs.variableToCoefficientMapping.clear();
    rhs.constantPart -= this->constantPart;
}

std::map<storm::expressions::Variable, double>::const_iterator LinearCoefficientVisitor::VariableCoefficients::begin() const {
    return this->variableToCoefficientMapping.begin();
}

std::map<storm::expressions::Variable, double>::const_iterator LinearCoefficientVisitor::VariableCoefficients::end() const {
    return this->variableToCoefficientMapping.end();
}

LinearCoefficientVisitor::VariableCoefficients LinearCoefficientVisitor::getLinearCoefficients(Expression const& expression) {
    return boost::any_cast<VariableCoefficients>(expression.getBaseExpression().accept(*this, boost::none));
}

boost::any LinearCoefficientVisitor::visit(IfThenElseExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
}

boost::any LinearCoefficientVisitor::visit(BinaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
}

boost::any LinearCoefficientVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    VariableCoefficients leftResult = boost::any_cast<VariableCoefficients>(expression.getFirstOperand()->accept(*this, data));
    VariableCoefficients rightResult = boost::any_cast<VariableCoefficients>(expression.getSecondOperand()->accept(*this, data));

    if (expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Plus) {
        leftResult += std::move(rightResult);
    } else if (expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Minus) {
        leftResult -= std::move(rightResult);
    } else if (expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Times) {
        leftResult *= std::move(rightResult);
    } else if (expression.getOperatorType() == BinaryNumericalFunctionExpression::OperatorType::Divide) {
        leftResult /= std::move(rightResult);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
    }
    return leftResult;
}

boost::any LinearCoefficientVisitor::visit(BinaryRelationExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
}

boost::any LinearCoefficientVisitor::visit(VariableExpression const& expression, boost::any const&) {
    VariableCoefficients coefficients;
    if (expression.getType().isNumericalType()) {
        coefficients.setCoefficient(expression.getVariable(), 1);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
    }
    return coefficients;
}

boost::any LinearCoefficientVisitor::visit(UnaryBooleanFunctionExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
}

boost::any LinearCoefficientVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    VariableCoefficients childResult = boost::any_cast<VariableCoefficients>(expression.getOperand()->accept(*this, data));

    if (expression.getOperatorType() == UnaryNumericalFunctionExpression::OperatorType::Minus) {
        childResult.negate();
        return childResult;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
    }
}

boost::any LinearCoefficientVisitor::visit(BooleanLiteralExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
}

boost::any LinearCoefficientVisitor::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return VariableCoefficients(static_cast<double>(expression.getValue()));
}

boost::any LinearCoefficientVisitor::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return VariableCoefficients(expression.getValueAsDouble());
}
}  // namespace expressions
}  // namespace storm
