#pragma once

#include "storm/expressions/ExpressionVisitor.h"
#include "storm/expressions/SyntacticalEqualityCheckVisitor.h"
#include "storm/languages/jani/expressions/JaniExpressions.h"
#include "storm/languages/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace expressions {
class JaniSyntacticalEqualityCheckVisitor : public SyntacticalEqualityCheckVisitor, public JaniExpressionVisitor {
   public:
    /*!
     * Creates a  visitor that checks if the given expressions are syntactically equal
     *
     * @param variableToExpressionMapping A mapping from variables to expressions.
     */
    JaniSyntacticalEqualityCheckVisitor();
    using SyntacticalEqualityCheckVisitor::visit;

    virtual boost::any visit(ValueArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ConstructorArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ArrayAccessExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(FunctionCallExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(TranscendentalNumberLiteralExpression const& expression, boost::any const& data) override;
};
}  // namespace expressions
}  // namespace storm
