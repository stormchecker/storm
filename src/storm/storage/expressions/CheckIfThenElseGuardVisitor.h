#pragma once

#include <set>

#include "storm/storage/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {

class Expression;
class Variable;

// Visits all sub-expressions and returns true if any of them is an IfThenElseExpression
// where the 'if' part depends on one of the variables in the set passed in the constructor.
class CheckIfThenElseGuardVisitor : public ExpressionVisitor {
   public:
    CheckIfThenElseGuardVisitor(std::set<storm::expressions::Variable> const& variables);

    bool check(storm::expressions::Expression const& expression);

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
    std::set<storm::expressions::Variable> const& variables;
};

}  // namespace expressions
}  // namespace storm
