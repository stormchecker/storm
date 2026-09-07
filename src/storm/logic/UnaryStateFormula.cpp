#include "UnaryStateFormula.h"

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
UnaryStateFormula::UnaryStateFormula(std::shared_ptr<Formula const> subformula) : subformula(subformula) {
    // Intentionally left empty.
}

bool UnaryStateFormula::isUnaryStateFormula() const {
    return true;
}

Formula const& UnaryStateFormula::getSubformula() const {
    return *subformula;
}

void UnaryStateFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    this->getSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void UnaryStateFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    this->getSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void UnaryStateFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    this->getSubformula().gatherReferencedRewardModels(referencedRewardModels);
}

void UnaryStateFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getSubformula().gatherUsedVariables(usedVariables);
}

}  // namespace logic
}  // namespace storm
