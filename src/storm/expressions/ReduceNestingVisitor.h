#pragma once

#include "storm/expressions/Expression.h"
#include "storm/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {
class ReduceNestingVisitor : public ExpressionVisitor {
   public:
    /*!
     * Creates a new reduce nesting visitor.
     */
    ReduceNestingVisitor();

    /*!
     * Reduces the nesting in the given expression
     *
     * @return A semantically equivalent expression with reduced nesting
     */
    Expression reduceNesting(Expression const& expression);

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

   private:
};
}  // namespace expressions
}  // namespace storm
