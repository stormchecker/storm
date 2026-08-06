#pragma once

#include <stack>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {
template<typename MapType>
class SubstitutionVisitor : public ExpressionVisitor {
   public:
    /*!
     * Creates a new substitution visitor that uses the given map to replace variables.
     *
     * @param variableToExpressionMapping A mapping from variables to expressions.
     */
    SubstitutionVisitor(MapType const& variableToExpressionMapping);

    /*!
     * Substitutes the identifiers in the given expression according to the previously given map and returns the
     * resulting expression.
     *
     * @param expression The expression in which to substitute the identifiers.
     * @return The expression in which all identifiers in the key set of the previously given mapping are
     * substituted with the mapped-to expressions.
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
    // A mapping of variables to expressions with which they shall be replaced.
    MapType const& variableToExpressionMapping;
};
}  // namespace expressions
}  // namespace storm
