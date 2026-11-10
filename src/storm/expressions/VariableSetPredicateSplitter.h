#pragma once

#include <set>
#include <vector>

#include "storm/expressions/ExpressionVisitor.h"

namespace storm {
namespace expressions {

class Variable;
class Expression;

class VariableSetPredicateSplitter : public ExpressionVisitor {
   public:
    VariableSetPredicateSplitter(std::set<storm::expressions::Variable> const& irrelevantVariables);

    std::vector<storm::expressions::Expression> split(storm::expressions::Expression const& expression);

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
    std::set<storm::expressions::Variable> irrelevantVariables;
    std::vector<storm::expressions::Expression> resultPredicates;
};

}  // namespace expressions
}  // namespace storm
