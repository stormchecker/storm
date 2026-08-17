#include "storm/storage/expressions/ExprtkExpressionEvaluator.h"

#include <string>

#include "storm/adapters/ExprttkAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
template<typename RationalType>
ExprtkExpressionEvaluatorBase<RationalType>::ExprtkExpressionEvaluatorBase(storm::expressions::ExpressionManager const& manager)
    : ExpressionEvaluatorBase<RationalType>(manager),
      parser(std::make_unique<exprtk::parser<ValueType>>()),
      symbolTable(std::make_unique<exprtk::symbol_table<ValueType>>()),
      booleanValues(manager.getNumberOfBooleanVariables()),
      integerValues(manager.getNumberOfIntegerVariables()),
      rationalValues(manager.getNumberOfRationalVariables()) {
    // Since some expressions are very long, we need to increase the stack depth of the exprtk parser.
    // Otherwise, we'll get
    //   ERR000 - Current stack depth X exceeds maximum allowed stack depth of Y
    // on some models.
    parser->settings().set_max_stack_depth(10000);
    for (auto const& variableTypePair : manager) {
        if (variableTypePair.second.isBooleanType()) {
            symbolTable->add_variable("v" + std::to_string(variableTypePair.first.getIndex()), this->booleanValues[variableTypePair.first.getOffset()]);
        } else if (variableTypePair.second.isIntegerType()) {
            symbolTable->add_variable("v" + std::to_string(variableTypePair.first.getIndex()), this->integerValues[variableTypePair.first.getOffset()]);
        } else if (variableTypePair.second.isRationalType()) {
            symbolTable->add_variable("v" + std::to_string(variableTypePair.first.getIndex()), this->rationalValues[variableTypePair.first.getOffset()]);
        }
    }
}

template<typename RationalType>
bool ExprtkExpressionEvaluatorBase<RationalType>::asBool(Expression const& expression) const {
    auto const& compiledExpression = getCompiledExpression(expression);
    return compiledExpression.value() == ValueType(1);
}

template<typename RationalType>
int_fast64_t ExprtkExpressionEvaluatorBase<RationalType>::asInt(Expression const& expression) const {
    auto const& compiledExpression = getCompiledExpression(expression);
    return static_cast<int_fast64_t>(compiledExpression.value());
}

template<typename RationalType>
typename ExprtkExpressionEvaluatorBase<RationalType>::CompiledExpressionType const& ExprtkExpressionEvaluatorBase<RationalType>::getCompiledExpression(
    storm::expressions::Expression const& expression) const {
    if (!expression.hasCompiledExpression() || !expression.getCompiledExpression().isExprtkCompiledExpression()) {
        CompiledExpressionType compiledExpression;
        compiledExpression.register_symbol_table(*symbolTable);
        bool parsingOk = parser->compile(ToExprtkStringVisitor().toString(expression), compiledExpression);
        STORM_LOG_THROW(parsingOk, storm::exceptions::UnexpectedException,
                        "Expression was not properly parsed by ExprTk: " << expression << ". (Returned error: " << parser->error() << ").");
        expression.setCompiledExpression(std::make_shared<ExprtkCompiledExpression>(compiledExpression));
    }
    return expression.getCompiledExpression().asExprtkCompiledExpression().getCompiledExpression();
}

template<typename RationalType>
void ExprtkExpressionEvaluatorBase<RationalType>::setBooleanValue(storm::expressions::Variable const& variable, bool value) {
    this->booleanValues[variable.getOffset()] = static_cast<ValueType>(value);
}

template<typename RationalType>
void ExprtkExpressionEvaluatorBase<RationalType>::setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) {
    this->integerValues[variable.getOffset()] = static_cast<ValueType>(value);
}

template<typename RationalType>
void ExprtkExpressionEvaluatorBase<RationalType>::setRationalValue(storm::expressions::Variable const& variable, double value) {
    this->rationalValues[variable.getOffset()] = static_cast<ValueType>(value);
}

ExprtkExpressionEvaluator::ExprtkExpressionEvaluator(storm::expressions::ExpressionManager const& manager) : ExprtkExpressionEvaluatorBase<double>(manager) {
    // Intentionally left empty.
}

double ExprtkExpressionEvaluator::asRational(Expression const& expression) const {
    auto const& compiledExpression = getCompiledExpression(expression);
    return static_cast<double>(compiledExpression.value());
}

template class ExprtkExpressionEvaluatorBase<double>;

template class ExprtkExpressionEvaluatorBase<RationalNumber>;
template class ExprtkExpressionEvaluatorBase<RationalFunction>;
}  // namespace expressions
}  // namespace storm
