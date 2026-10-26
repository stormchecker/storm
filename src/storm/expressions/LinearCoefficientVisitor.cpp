#include "storm/expressions/LinearCoefficientVisitor.h"

#include <iterator>

#include "storm/expressions/Expressions.h"

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

template<typename ValueType>
LinearCoefficientVisitor<ValueType>::VariableCoefficients::VariableCoefficients(ValueType constantPart)
    : variableToCoefficientMapping(), constantPart(std::move(constantPart)) {
    // Intentionally left empty.
}

template<typename ValueType>
void LinearCoefficientVisitor<ValueType>::VariableCoefficients::removeZeroCoefficients() {
    for (auto it = variableToCoefficientMapping.begin(); it != variableToCoefficientMapping.end();) {
        it = storm::utility::isZero(it->second) ? variableToCoefficientMapping.erase(it) : std::next(it);
    }
}

template<typename ValueType>
typename LinearCoefficientVisitor<ValueType>::VariableCoefficients& LinearCoefficientVisitor<ValueType>::VariableCoefficients::operator+=(
    VariableCoefficients&& other) {
    for (auto const& otherVariableCoefficientPair : other.variableToCoefficientMapping) {
        auto [it, inserted] = this->variableToCoefficientMapping.try_emplace(otherVariableCoefficientPair.first, otherVariableCoefficientPair.second);
        if (!inserted) {
            it->second += otherVariableCoefficientPair.second;
        }
    }
    constantPart += other.constantPart;
    removeZeroCoefficients();
    return *this;
}

template<typename ValueType>
typename LinearCoefficientVisitor<ValueType>::VariableCoefficients& LinearCoefficientVisitor<ValueType>::VariableCoefficients::operator-=(
    VariableCoefficients&& other) {
    other.negate();
    return *this += std::move(other);
}

template<typename ValueType>
typename LinearCoefficientVisitor<ValueType>::VariableCoefficients& LinearCoefficientVisitor<ValueType>::VariableCoefficients::operator*=(
    VariableCoefficients&& other) {
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
    removeZeroCoefficients();
    return *this;
}

template<typename ValueType>
typename LinearCoefficientVisitor<ValueType>::VariableCoefficients& LinearCoefficientVisitor<ValueType>::VariableCoefficients::operator/=(
    VariableCoefficients&& other) {
    STORM_LOG_THROW(other.variableToCoefficientMapping.size() == 0 && !storm::utility::isZero(other.constantPart), storm::exceptions::InvalidArgumentException,
                    "Expression is non-linear or divides by zero.");
    for (auto& variableCoefficientPair : this->variableToCoefficientMapping) {
        variableCoefficientPair.second /= other.constantPart;
    }
    constantPart /= other.constantPart;
    return *this;
}

template<typename ValueType>
void LinearCoefficientVisitor<ValueType>::VariableCoefficients::negate() {
    for (auto& variableCoefficientPair : variableToCoefficientMapping) {
        variableCoefficientPair.second = -variableCoefficientPair.second;
    }
    constantPart = -constantPart;
}

template<typename ValueType>
void LinearCoefficientVisitor<ValueType>::VariableCoefficients::setCoefficient(storm::expressions::Variable const& variable, ValueType const& coefficient) {
    variableToCoefficientMapping.insert_or_assign(variable, coefficient);
}

template<typename ValueType>
ValueType LinearCoefficientVisitor<ValueType>::VariableCoefficients::getCoefficient(storm::expressions::Variable const& variable) {
    auto it = variableToCoefficientMapping.find(variable);
    return it == variableToCoefficientMapping.end() ? storm::utility::zero<ValueType>() : it->second;
}

template<typename ValueType>
size_t LinearCoefficientVisitor<ValueType>::VariableCoefficients::size() const {
    return variableToCoefficientMapping.size();
}

template<typename ValueType>
ValueType const& LinearCoefficientVisitor<ValueType>::VariableCoefficients::getConstantPart() const {
    return this->constantPart;
}

template<typename ValueType>
void LinearCoefficientVisitor<ValueType>::VariableCoefficients::separateVariablesFromConstantPart(VariableCoefficients& rhs) {
    for (auto const& rhsVariableCoefficientPair : rhs.variableToCoefficientMapping) {
        auto [it, inserted] = this->variableToCoefficientMapping.try_emplace(rhsVariableCoefficientPair.first, -rhsVariableCoefficientPair.second);
        if (!inserted) {
            it->second -= rhsVariableCoefficientPair.second;
        }
    }
    removeZeroCoefficients();
    rhs.variableToCoefficientMapping.clear();
    rhs.constantPart -= this->constantPart;
}

template<typename ValueType>
typename std::map<storm::expressions::Variable, ValueType>::const_iterator LinearCoefficientVisitor<ValueType>::VariableCoefficients::begin() const {
    return this->variableToCoefficientMapping.begin();
}

template<typename ValueType>
typename std::map<storm::expressions::Variable, ValueType>::const_iterator LinearCoefficientVisitor<ValueType>::VariableCoefficients::end() const {
    return this->variableToCoefficientMapping.end();
}

template<typename ValueType>
std::optional<typename LinearCoefficientVisitor<ValueType>::VariableCoefficients> LinearCoefficientVisitor<ValueType>::tryGetLinearCoefficients(
    Expression const& expression) {
    return boost::any_cast<Result>(expression.getBaseExpression().accept(*this, boost::none));
}

template<typename ValueType>
typename LinearCoefficientVisitor<ValueType>::VariableCoefficients LinearCoefficientVisitor<ValueType>::getLinearCoefficients(Expression const& expression) {
    auto result = tryGetLinearCoefficients(expression);
    STORM_LOG_THROW(result.has_value(), storm::exceptions::InvalidArgumentException, "Expression is non-linear.");
    return std::move(result.value());
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(IfThenElseExpression const&, boost::any const&) {
    return Result{};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(BinaryBooleanFunctionExpression const&, boost::any const&) {
    return Result{};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    Result left = boost::any_cast<Result>(expression.getFirstOperand()->accept(*this, data));
    if (!left) {
        return Result{};
    }
    Result right = boost::any_cast<Result>(expression.getSecondOperand()->accept(*this, data));
    if (!right) {
        return Result{};
    }

    switch (expression.getOperatorType()) {
        case BinaryNumericalFunctionExpression::OperatorType::Plus:
            *left += std::move(*right);
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Minus:
            *left -= std::move(*right);
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Times:
            if (left->size() > 0 && right->size() > 0) {
                return Result{};
            }
            *left *= std::move(*right);
            break;
        case BinaryNumericalFunctionExpression::OperatorType::Divide:
            if (right->size() > 0 || storm::utility::isZero(right->getConstantPart())) {
                return Result{};
            }
            *left /= std::move(*right);
            break;
        default:
            return Result{};
    }
    return left;
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(BinaryRelationExpression const&, boost::any const&) {
    return Result{};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(VariableExpression const& expression, boost::any const&) {
    if (!expression.getType().isNumericalType()) {
        return Result{};
    }
    VariableCoefficients coefficients;
    coefficients.setCoefficient(expression.getVariable(), storm::utility::one<ValueType>());
    return Result{std::move(coefficients)};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(UnaryBooleanFunctionExpression const&, boost::any const&) {
    return Result{};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    Result child = boost::any_cast<Result>(expression.getOperand()->accept(*this, data));
    if (!child || expression.getOperatorType() != UnaryNumericalFunctionExpression::OperatorType::Minus) {
        return Result{};
    }
    child->negate();
    return child;
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(BooleanLiteralExpression const&, boost::any const&) {
    return Result{};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(IntegerLiteralExpression const& expression, boost::any const&) {
    return Result{VariableCoefficients(storm::utility::convertNumber<ValueType>(expression.getValue()))};
}

template<typename ValueType>
boost::any LinearCoefficientVisitor<ValueType>::visit(RationalLiteralExpression const& expression, boost::any const&) {
    return Result{VariableCoefficients(storm::utility::convertNumber<ValueType>(expression.getValue()))};
}

template class LinearCoefficientVisitor<double>;
template class LinearCoefficientVisitor<storm::RationalNumber>;
// The coefficients of rational functions use a different type if exact arithmetic and rational functions use different libraries.
#if (defined(STORM_USE_CLN_EA) && !defined(STORM_USE_CLN_RF)) || (!defined(STORM_USE_CLN_EA) && defined(STORM_USE_CLN_RF))
template class LinearCoefficientVisitor<storm::RationalFunctionCoefficient>;
#endif
}  // namespace expressions
}  // namespace storm
