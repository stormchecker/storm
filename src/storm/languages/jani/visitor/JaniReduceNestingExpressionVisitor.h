#pragma once

#include "JaniExpressionVisitor.h"
#include "storm/expressions/ReduceNestingVisitor.h"
#include "storm/languages/jani/expressions/JaniExpressions.h"

namespace storm {
namespace jani {
storm::expressions::Expression reduceNestingInJaniExpression(storm::expressions::Expression const& expression);
}

namespace expressions {

class JaniReduceNestingExpressionVisitor : public ReduceNestingVisitor, public JaniExpressionVisitor {
   public:
    JaniReduceNestingExpressionVisitor();
    using ReduceNestingVisitor::visit;

    virtual boost::any visit(ValueArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ConstructorArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ArrayAccessExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(FunctionCallExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(TranscendentalNumberLiteralExpression const& expression, boost::any const& data) override;
};
}  // namespace expressions
}  // namespace storm