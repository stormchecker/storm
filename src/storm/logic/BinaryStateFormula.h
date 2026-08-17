#pragma once

#include "storm/logic/StateFormula.h"

namespace storm {
namespace logic {
class BinaryStateFormula : public StateFormula {
   public:
    BinaryStateFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula);

    virtual ~BinaryStateFormula() {
        // Intentionally left empty.
    }

    virtual bool isBinaryStateFormula() const override;

    Formula const& getLeftSubformula() const;
    Formula const& getRightSubformula() const;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

   private:
    std::shared_ptr<Formula const> leftSubformula;
    std::shared_ptr<Formula const> rightSubformula;
};
}  // namespace logic
}  // namespace storm
