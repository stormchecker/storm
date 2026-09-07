#include "BinaryPathFormula.h"

namespace storm {
namespace logic {
BinaryPathFormula::BinaryPathFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula)
    : leftSubformula(leftSubformula), rightSubformula(rightSubformula) {
    // Intentionally left empty.
}

bool BinaryPathFormula::isBinaryPathFormula() const {
    return true;
}

Formula const& BinaryPathFormula::getLeftSubformula() const {
    return *leftSubformula;
}

Formula const& BinaryPathFormula::getRightSubformula() const {
    return *rightSubformula;
}

void BinaryPathFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    this->getLeftSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    this->getRightSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void BinaryPathFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    this->getLeftSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
    this->getRightSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void BinaryPathFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    this->getLeftSubformula().gatherReferencedRewardModels(referencedRewardModels);
    this->getRightSubformula().gatherReferencedRewardModels(referencedRewardModels);
}

void BinaryPathFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getLeftSubformula().gatherUsedVariables(usedVariables);
    this->getRightSubformula().gatherUsedVariables(usedVariables);
}

bool BinaryPathFormula::hasQualitativeResult() const {
    return false;
}

bool BinaryPathFormula::hasQuantitativeResult() const {
    return true;
}
}  // namespace logic
}  // namespace storm
