#include "storm/logic/CvarFormula.h"

#include <boost/any.hpp>
#include <ostream>
#include <utility>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

CvarFormula::CvarFormula(storm::RationalNumber const& alpha, std::shared_ptr<Formula const> subformula) : alpha(alpha), subformula(std::move(subformula)) {
    STORM_LOG_THROW(this->subformula != nullptr, storm::exceptions::InvalidArgumentException, "A CVaR formula requires a subformula.");
    STORM_LOG_THROW(storm::utility::zero<storm::RationalNumber>() < this->alpha && this->alpha < storm::utility::one<storm::RationalNumber>(),
                    storm::exceptions::InvalidArgumentException, "The CVaR alpha must be in the open interval (0, 1).");
}

CvarFormula::~CvarFormula() {
    // Intentionally left empty.
}

bool CvarFormula::isCvarFormula() const {
    return true;
}

bool CvarFormula::hasQuantitativeResult() const {
    return true;
}

bool CvarFormula::hasNumericalResult() const {
    return true;
}

bool CvarFormula::hasMultiDimensionalResult() const {
    return false;
}

storm::RationalNumber const& CvarFormula::getAlpha() const {
    return alpha;
}

Formula const& CvarFormula::getSubformula() const {
    return *subformula;
}

boost::any CvarFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void CvarFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    subformula->gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void CvarFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    subformula->gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void CvarFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    subformula->gatherReferencedRewardModels(referencedRewardModels);
}

void CvarFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    subformula->gatherUsedVariables(usedVariables);
}

std::ostream& CvarFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    out << "cvar(" << alpha << ", ";
    subformula->writeToStream(out);
    out << ")";
    return out;
}

}  // namespace logic
}  // namespace storm
