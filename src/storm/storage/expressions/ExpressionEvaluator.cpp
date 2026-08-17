#include "storm/storage/expressions/ExpressionEvaluator.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"

namespace storm {
namespace expressions {
ExpressionEvaluator<double>::ExpressionEvaluator(storm::expressions::ExpressionManager const& manager) : ExprtkExpressionEvaluator(manager) {
    // Intentionally left empty.
}

template<typename RationalType>
ExpressionEvaluatorWithVariableToExpressionMap<RationalType>::ExpressionEvaluatorWithVariableToExpressionMap(
    storm::expressions::ExpressionManager const& manager)
    : ExprtkExpressionEvaluatorBase<RationalType>(manager) {
    // Intentionally left empty.
}

template<typename RationalType>
void ExpressionEvaluatorWithVariableToExpressionMap<RationalType>::setBooleanValue(storm::expressions::Variable const& variable, bool value) {
    ExprtkExpressionEvaluatorBase<RationalType>::setBooleanValue(variable, value);
    this->variableToExpressionMap[variable] = this->getManager().boolean(value);
}

template<typename RationalType>
void ExpressionEvaluatorWithVariableToExpressionMap<RationalType>::setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) {
    ExprtkExpressionEvaluatorBase<RationalType>::setIntegerValue(variable, value);
    this->variableToExpressionMap[variable] = this->getManager().integer(value);
}

template<typename RationalType>
void ExpressionEvaluatorWithVariableToExpressionMap<RationalType>::setRationalValue(storm::expressions::Variable const& variable, double value) {
    ExprtkExpressionEvaluatorBase<RationalType>::setRationalValue(variable, value);
    this->variableToExpressionMap[variable] = this->getManager().rational(value);
}

ExpressionEvaluator<RationalNumber>::ExpressionEvaluator(storm::expressions::ExpressionManager const& manager)
    : ExprtkExpressionEvaluatorBase<RationalNumber>(manager), rationalNumberVisitor(*this) {
    // Intentionally left empty.
}

void ExpressionEvaluator<RationalNumber>::setBooleanValue(storm::expressions::Variable const& variable, bool value) {
    ExprtkExpressionEvaluatorBase<RationalNumber>::setBooleanValue(variable, value);

    // Not forwarding value of variable to rational number visitor as it cannot treat boolean variables anyway.
}

void ExpressionEvaluator<RationalNumber>::setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) {
    ExprtkExpressionEvaluatorBase<RationalNumber>::setIntegerValue(variable, value);
    rationalNumberVisitor.setMapping(variable, storm::utility::convertNumber<RationalNumber>(value));
}

void ExpressionEvaluator<RationalNumber>::setRationalValue(storm::expressions::Variable const& variable, double value) {
    ExprtkExpressionEvaluatorBase<RationalNumber>::setRationalValue(variable, value);
    rationalNumberVisitor.setMapping(variable, storm::utility::convertNumber<RationalNumber>(value));
}

void ExpressionEvaluator<RationalNumber>::setRationalValue(storm::expressions::Variable const& variable, RationalNumber const& value) {
    ExprtkExpressionEvaluatorBase<RationalNumber>::setRationalValue(variable, storm::utility::convertNumber<double>(value));
    rationalNumberVisitor.setMapping(variable, value);
}

RationalNumber ExpressionEvaluator<RationalNumber>::asRational(Expression const& expression) const {
    RationalNumber result = this->rationalNumberVisitor.toRationalNumber(expression);
    return result;
}

ExpressionEvaluator<RationalFunction>::ExpressionEvaluator(storm::expressions::ExpressionManager const& manager)
    : ExprtkExpressionEvaluatorBase<RationalFunction>(manager), rationalFunctionVisitor(*this) {
    // Intentionally left empty.
}

void ExpressionEvaluator<RationalFunction>::setBooleanValue(storm::expressions::Variable const& variable, bool value) {
    ExprtkExpressionEvaluatorBase<RationalFunction>::setBooleanValue(variable, value);

    // Not forwarding value of variable to rational number visitor as it cannot treat boolean variables anyway.
}

void ExpressionEvaluator<RationalFunction>::setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) {
    ExprtkExpressionEvaluatorBase<RationalFunction>::setIntegerValue(variable, value);
    rationalFunctionVisitor.setMapping(variable, storm::utility::convertNumber<RationalFunction>(value));
}

void ExpressionEvaluator<RationalFunction>::setRationalValue(storm::expressions::Variable const& variable, double value) {
    ExprtkExpressionEvaluatorBase<RationalFunction>::setRationalValue(variable, value);
    rationalFunctionVisitor.setMapping(variable, storm::utility::convertNumber<RationalFunction>(value));
}

void ExpressionEvaluator<RationalFunction>::setRationalValue(storm::expressions::Variable const& variable, RationalFunction const& value) {
    STORM_LOG_ASSERT(storm::utility::isConstant(value), "Value for rational variable is not a constant.");
    ExprtkExpressionEvaluatorBase<RationalFunction>::setRationalValue(variable, storm::utility::convertNumber<double>(value));
    rationalFunctionVisitor.setMapping(variable, value);
}

RationalFunction ExpressionEvaluator<RationalFunction>::asRational(Expression const& expression) const {
    return this->rationalFunctionVisitor.toRationalFunction(expression);
}

template class ExpressionEvaluatorWithVariableToExpressionMap<RationalNumber>;
template class ExpressionEvaluatorWithVariableToExpressionMap<RationalFunction>;
}  // namespace expressions
}  // namespace storm
