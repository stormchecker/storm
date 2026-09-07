#include "ConditionalFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
ConditionalFormula::ConditionalFormula(std::shared_ptr<Formula const> const& subformula, std::shared_ptr<Formula const> const& conditionFormula,
                                       FormulaContext context)
    : subformula(subformula), conditionFormula(conditionFormula), context(context) {
    STORM_LOG_THROW(context == FormulaContext::Probability || context == FormulaContext::Reward, storm::exceptions::InvalidPropertyException,
                    "Invalid context for formula.");
}

Formula const& ConditionalFormula::getSubformula() const {
    return *subformula;
}

Formula const& ConditionalFormula::getConditionFormula() const {
    return *conditionFormula;
}

FormulaContext const& ConditionalFormula::getContext() const {
    return context;
}

bool ConditionalFormula::isConditionalProbabilityFormula() const {
    return context == FormulaContext::Probability;
}

bool ConditionalFormula::isConditionalRewardFormula() const {
    return context == FormulaContext::Reward;
}

boost::any ConditionalFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void ConditionalFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    this->getSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    this->getConditionFormula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void ConditionalFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    this->getSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
    this->getConditionFormula().gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void ConditionalFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    this->getSubformula().gatherReferencedRewardModels(referencedRewardModels);
    this->getConditionFormula().gatherReferencedRewardModels(referencedRewardModels);
}

void ConditionalFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getSubformula().gatherUsedVariables(usedVariables);
    this->getConditionFormula().gatherUsedVariables(usedVariables);
}

bool ConditionalFormula::hasQualitativeResult() const {
    return false;
}

bool ConditionalFormula::hasQuantitativeResult() const {
    return true;
}

std::ostream& ConditionalFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    this->getSubformula().writeToStream(out, true);
    out << " || ";
    this->getConditionFormula().writeToStream(out, true);
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
