#pragma once
#include <stack>

#include "storm/expressions/Expression.h"
#include "storm/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {
class RestrictSyntaxVisitor : public ExpressionVisitor {
   public:
    /*!
     * Creates a new simplification visitor that replaces predicates by other (simpler?) predicates.
     *
     *  Configuration:
     *  Currently, the visitor only replaces nonstandard predicates
     *
     */
    RestrictSyntaxVisitor();

    /*!
     * Simplifies based on the configuration.
     */
    Expression substitute(Expression const& expression);

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
    virtual boost::any visit(PredicateExpression const& expression, boost::any const& data) override;

   protected:
    //
};
}  // namespace expressions
}  // namespace storm
