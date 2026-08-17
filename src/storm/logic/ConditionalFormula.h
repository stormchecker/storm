#pragma once

#include "storm/logic/BinaryPathFormula.h"
#include "storm/logic/FormulaContext.h"

namespace storm {
namespace logic {
class ConditionalFormula : public Formula {
   public:
    ConditionalFormula(std::shared_ptr<Formula const> const& subformula, std::shared_ptr<Formula const> const& conditionFormula,
                       FormulaContext context = FormulaContext::Probability);

    virtual ~ConditionalFormula() {
        // Intentionally left empty.
    }

    Formula const& getSubformula() const;
    Formula const& getConditionFormula() const;
    FormulaContext const& getContext() const;

    virtual bool isConditionalProbabilityFormula() const override;
    virtual bool isConditionalRewardFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

   private:
    std::shared_ptr<Formula const> subformula;
    std::shared_ptr<Formula const> conditionFormula;
    FormulaContext context;
};
}  // namespace logic
}  // namespace storm
