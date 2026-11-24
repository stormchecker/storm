#pragma once

#include "storm/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {

class Expression;

class SyntacticalEqualityCheckVisitor : public ExpressionVisitor {
   public:
    bool isSyntacticallyEqual(storm::expressions::Expression const& expression1, storm::expressions::Expression const& expression2);

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
