#pragma once

#include <unordered_map>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExprtkExpressionEvaluator.h"
#include "storm/storage/expressions/ToRationalFunctionVisitor.h"
#include "storm/storage/expressions/ToRationalNumberVisitor.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {
template<typename RationalType>
class ExpressionEvaluator;

template<>
class ExpressionEvaluator<double> : public ExprtkExpressionEvaluator {
   public:
    ExpressionEvaluator(storm::expressions::ExpressionManager const& manager);
};

template<typename RationalType>
class ExpressionEvaluatorWithVariableToExpressionMap : public ExprtkExpressionEvaluatorBase<RationalType> {
   public:
    ExpressionEvaluatorWithVariableToExpressionMap(storm::expressions::ExpressionManager const& manager);

    void setBooleanValue(storm::expressions::Variable const& variable, bool value) override;
    void setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) override;
    void setRationalValue(storm::expressions::Variable const& variable, double value) override;

   protected:
    // A mapping of variables to their expressions.
    std::unordered_map<storm::expressions::Variable, storm::expressions::Expression> variableToExpressionMap;
};

template<>
class ExpressionEvaluator<RationalNumber> : public ExprtkExpressionEvaluatorBase<RationalNumber> {
   public:
    ExpressionEvaluator(storm::expressions::ExpressionManager const& manager);

    void setBooleanValue(storm::expressions::Variable const& variable, bool value) override;
    void setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) override;
    void setRationalValue(storm::expressions::Variable const& variable, double value) override;

    // Sets a rational value from a RationalNumber.
    // Note: If an expression contains the given variable and is evaluated to int or bool, the value is internally considered as a double.
    void setRationalValue(storm::expressions::Variable const& variable, storm::RationalNumber const& value);

    RationalNumber asRational(Expression const& expression) const override;

   private:
    // A visitor that can be used to translate expressions to rational numbers.
    mutable ToRationalNumberVisitor<RationalNumber> rationalNumberVisitor;
};

template<>
class ExpressionEvaluator<RationalFunction> : public ExprtkExpressionEvaluatorBase<RationalFunction> {
   public:
    ExpressionEvaluator(storm::expressions::ExpressionManager const& manager);

    void setBooleanValue(storm::expressions::Variable const& variable, bool value) override;
    void setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) override;
    void setRationalValue(storm::expressions::Variable const& variable, double value) override;

    // Sets a rational value from a RationalFunction. The function needs to be constant.
    // Note: If an expression contains the given variable and is evaluated to int or bool, the value is internally considered as a double.
    void setRationalValue(storm::expressions::Variable const& variable, storm::RationalFunction const& value);

    RationalFunction asRational(Expression const& expression) const override;

   private:
    // A visitor that can be used to translate expressions to rational functions.
    mutable ToRationalFunctionVisitor<RationalFunction> rationalFunctionVisitor;
};
}  // namespace expressions
}  // namespace storm
