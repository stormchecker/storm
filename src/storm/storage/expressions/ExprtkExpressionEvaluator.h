#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "storm/storage/expressions/ExpressionEvaluatorBase.h"
#include "storm/storage/expressions/ExprtkCompiledExpression.h"
#include "storm/storage/expressions/ToExprtkStringVisitor.h"

namespace storm {
namespace expressions {

template<typename RationalType>
class ExprtkExpressionEvaluatorBase : public ExpressionEvaluatorBase<RationalType> {
   public:
    ExprtkExpressionEvaluatorBase(storm::expressions::ExpressionManager const& manager);

    bool asBool(Expression const& expression) const override;
    int_fast64_t asInt(Expression const& expression) const override;

    void setBooleanValue(storm::expressions::Variable const& variable, bool value) override;
    void setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) override;
    void setRationalValue(storm::expressions::Variable const& variable, double value) override;

   protected:
    typedef double ValueType;
    typedef ExprtkCompiledExpression::CompiledExpressionType CompiledExpressionType;

    /*!
     * Retrieves a compiled version of the given expression.
     *
     * @param expression The expression that is to be compiled.
     */
    CompiledExpressionType const& getCompiledExpression(storm::expressions::Expression const& expression) const;

    // The parser used.
    mutable std::unique_ptr<exprtk::parser<ValueType>> parser;

    // The symbol table used.
    mutable std::unique_ptr<exprtk::symbol_table<ValueType>> symbolTable;

    // The actual data that is fed into the expression.
    std::vector<ValueType> booleanValues;
    std::vector<ValueType> integerValues;
    std::vector<ValueType> rationalValues;
};

class ExprtkExpressionEvaluator : public ExprtkExpressionEvaluatorBase<double> {
   public:
    /*!
     * Creates an expression evaluator that is capable of evaluating expressions managed by the given manager.
     *
     * @param manager The manager responsible for the expressions.
     */
    ExprtkExpressionEvaluator(storm::expressions::ExpressionManager const& manager);

    double asRational(Expression const& expression) const override;
};
}  // namespace expressions
}  // namespace storm
