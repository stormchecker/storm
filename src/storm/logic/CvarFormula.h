#pragma once

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/StateFormula.h"

namespace storm {
namespace logic {

class CvarFormula : public StateFormula {
   public:
    CvarFormula(storm::RationalNumber const& alpha, std::shared_ptr<Formula const> subformula);

    virtual ~CvarFormula();

    virtual bool isCvarFormula() const override;

    virtual bool hasQuantitativeResult() const override;
    virtual bool hasNumericalResult() const;
    virtual bool hasMultiDimensionalResult() const;

    storm::RationalNumber const& getAlpha() const;
    Formula const& getSubformula() const;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;
    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    storm::RationalNumber alpha;
    std::shared_ptr<Formula const> subformula;
};

}  // namespace logic
}  // namespace storm
