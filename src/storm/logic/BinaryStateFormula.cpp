#include "BinaryStateFormula.h"

namespace storm {
namespace logic {
BinaryStateFormula::BinaryStateFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula)
    : leftSubformula(leftSubformula), rightSubformula(rightSubformula) {
    // Intentionally left empty.
}

bool BinaryStateFormula::isBinaryStateFormula() const {
    return true;
}

Formula const& BinaryStateFormula::getLeftSubformula() const {
    return *leftSubformula;
}

Formula const& BinaryStateFormula::getRightSubformula() const {
    return *rightSubformula;
}

void BinaryStateFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    this->getLeftSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    this->getRightSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void BinaryStateFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    this->getLeftSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
    this->getRightSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void BinaryStateFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    this->getLeftSubformula().gatherReferencedRewardModels(referencedRewardModels);
    this->getRightSubformula().gatherReferencedRewardModels(referencedRewardModels);
}

void BinaryStateFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getLeftSubformula().gatherUsedVariables(usedVariables);
    this->getRightSubformula().gatherUsedVariables(usedVariables);
}
}  // namespace logic
}  // namespace storm
