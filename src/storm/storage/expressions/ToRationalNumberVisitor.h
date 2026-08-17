#pragma once

#include <boost/optional.hpp>
#include <unordered_map>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionEvaluatorBase.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {

template<typename RationalNumberType>
class ToRationalNumberVisitor : public ExpressionVisitor {
   public:
    ToRationalNumberVisitor();
    ToRationalNumberVisitor(ExpressionEvaluatorBase<RationalNumberType> const& evaluator);

    RationalNumberType toRationalNumber(Expression const& expression);

    virtual boost::any visit(IfThenElseExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryRelationExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(VariableExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BooleanLiteralExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(IntegerLiteralExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(RationalLiteralExpression const& expression, boost::any const& data) override;

    void setMapping(storm::expressions::Variable const& variable, RationalNumberType const& value);

   private:
    std::unordered_map<storm::expressions::Variable, RationalNumberType> valueMapping;

    // An optional reference to an expression evaluator (mainly for resolving the boolean condition in IfThenElse expressions)
    boost::optional<ExpressionEvaluatorBase<RationalNumberType> const&> evaluator;
};
}  // namespace expressions
}  // namespace storm
