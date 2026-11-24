#pragma once

#include "storm/expressions/ExpressionVisitor.h"
#include "storm/languages/jani/expressions/JaniExpressions.h"

namespace storm {
namespace expressions {
class JaniExpressionVisitor {
   public:
    virtual boost::any visit(ValueArrayExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(ConstructorArrayExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(ArrayAccessExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(FunctionCallExpression const& expression, boost::any const& data) = 0;
    virtual boost::any visit(TranscendentalNumberLiteralExpression const& expression, boost::any const& data) = 0;
};
}  // namespace expressions
}  // namespace storm
