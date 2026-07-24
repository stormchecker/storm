#pragma once

#include "storm/logic/StateFormula.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {
class AtomicExpressionFormula : public StateFormula {
   public:
    AtomicExpressionFormula(storm::expressions::Expression const& expression);

    virtual ~AtomicExpressionFormula() {
        // Intentionally left empty.
    }

    virtual bool isAtomicExpressionFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    storm::expressions::Expression const& getExpression() const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

   private:
    // The atomic expression represented by this node in the formula tree.
    storm::expressions::Expression expression;
};
}  // namespace logic
}  // namespace storm
