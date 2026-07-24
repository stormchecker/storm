#pragma once

#include <memory>

#include "storm/logic/PathFormula.h"

namespace storm {
namespace logic {
class BinaryPathFormula : public PathFormula {
   public:
    BinaryPathFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula);

    virtual ~BinaryPathFormula() {
        // Intentionally left empty.
    }

    virtual bool isBinaryPathFormula() const override;

    Formula const& getLeftSubformula() const;
    Formula const& getRightSubformula() const;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

   private:
    std::shared_ptr<Formula const> leftSubformula;
    std::shared_ptr<Formula const> rightSubformula;
};
}  // namespace logic
}  // namespace storm
