#pragma once

#include <cstddef>

#include "storm/storage/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {

class Expression;
class BaseExpression;

/*!
 * Computes a hash of an expression such that syntactically equal expressions hash to the same value.
 *
 * This is the hash counterpart of SyntacticalEqualityCheckVisitor: for any two expressions that satisfy
 * isSyntacticallyEqual, this visitor returns the same hash. The converse does not hold, so a hash can be
 * used to speed up the lookup of syntactically equal expressions, but a hash equality alone does not
 * establish syntactic equality.
 */
class HashVisitor : public ExpressionVisitor {
   private:
    /*!
     * Computes the hash of a nested expression. The accessors of the expression classes hand out
     * BaseExpression references, so this dispatches on those.
     */
    std::size_t hashExpression(BaseExpression const& expression);

   public:
    /*!
     * Computes the hash of the given expression. Two syntactically equal expressions always yield the same hash.
     */
    std::size_t hash(storm::expressions::Expression const& expression);

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
};

}  // namespace expressions
}  // namespace storm
