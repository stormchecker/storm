#pragma once

#include <boost/any.hpp>
#include <boost/none.hpp>

namespace storm {
namespace expressions {
// Forward-declare all expression classes.
class IfThenElseExpression;
class BinaryBooleanFunctionExpression;
class BinaryNumericalFunctionExpression;
class BinaryRelationExpression;
class VariableExpression;
class UnaryBooleanFunctionExpression;
class UnaryNumericalFunctionExpression;
class BooleanLiteralExpression;
class IntegerLiteralExpression;
class RationalLiteralExpression;
class PredicateExpression;

class ExpressionVisitor {
   public:
    virtual ~ExpressionVisitor() = default;

    virtual boost::any visit(IfThenElseExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(BinaryRelationExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(VariableExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(BooleanLiteralExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(IntegerLiteralExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(RationalLiteralExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(PredicateExpression const& expression, boost::any const& data);
};
}  // namespace expressions
}  // namespace storm
