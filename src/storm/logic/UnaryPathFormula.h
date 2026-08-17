#pragma once

#include <memory>

#include "storm/logic/PathFormula.h"

namespace storm {
namespace logic {
class UnaryPathFormula : public PathFormula {
   public:
    UnaryPathFormula(std::shared_ptr<Formula const> const& subformula);

    virtual ~UnaryPathFormula() {
        // Intentionally left empty.
    }

    virtual bool isUnaryPathFormula() const override;

    Formula const& getSubformula() const;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

   private:
    std::shared_ptr<Formula const> subformula;
};
}  // namespace logic
}  // namespace storm
