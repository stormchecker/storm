#include "storm/logic/MultiObjectiveFormula.h"
#include <algorithm>
#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

MultiObjectiveFormula::MultiObjectiveFormula(std::vector<std::shared_ptr<Formula const>> const& subformulas, Type type) : subformulas(subformulas), type(type) {
    // Intentionally left empty
}

MultiObjectiveFormula::~MultiObjectiveFormula() {
    // Intentionally left empty
}

bool MultiObjectiveFormula::isMultiObjectiveFormula() const {
    return true;
}

bool MultiObjectiveFormula::isTradeoff() const {
    return this->type == Type::Tradeoff;
}

bool MultiObjectiveFormula::isLexicographic() const {
    return this->type == Type::Lexicographic;
}

MultiObjectiveFormula::Type MultiObjectiveFormula::getType() const {
    return this->type;
}

bool MultiObjectiveFormula::hasQualitativeResult() const {
    return std::all_of(subformulas.begin(), subformulas.end(), [](auto const& f) { return f->hasQualitativeResult(); });
}

bool MultiObjectiveFormula::hasQuantitativeResult() const {
    return !hasQualitativeResult();
}

bool MultiObjectiveFormula::hasNumericalResult() const {
    auto numQuant = std::count_if(subformulas.begin(), subformulas.end(), [](auto const& f) { return f->hasQuantitativeResult(); });
    return numQuant == 1;
}

bool MultiObjectiveFormula::hasMultiDimensionalResult() const {
    auto numQuant = std::count_if(subformulas.begin(), subformulas.end(), [](auto const& f) { return f->hasQuantitativeResult(); });
    return numQuant > 1;
}

Formula const& MultiObjectiveFormula::getSubformula(uint_fast64_t index) const {
    STORM_LOG_THROW(index < getNumberOfSubformulas(), storm::exceptions::InvalidArgumentException,
                    "Tried to access subformula with index " << index << " but there are only " << this->getNumberOfSubformulas() << " subformulas.");
    return *this->subformulas[index];
}

uint_fast64_t MultiObjectiveFormula::getNumberOfSubformulas() const {
    return this->subformulas.size();
}

std::vector<std::shared_ptr<Formula const>> const& MultiObjectiveFormula::getSubformulas() const {
    return this->subformulas;
}

boost::any MultiObjectiveFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void MultiObjectiveFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    for (auto const& subformula : this->subformulas) {
        subformula->gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    }
}

void MultiObjectiveFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    for (auto const& subformula : this->subformulas) {
        subformula->gatherAtomicLabelFormulas(atomicLabelFormulas);
    }
}

void MultiObjectiveFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    for (auto const& subformula : this->subformulas) {
        subformula->gatherReferencedRewardModels(referencedRewardModels);
    }
}

void MultiObjectiveFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    for (auto const& subformula : this->subformulas) {
        subformula->gatherUsedVariables(usedVariables);
    }
}

std::ostream& MultiObjectiveFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "multi";
    if (isLexicographic()) {
        out << "lex";
    }
    out << "(";
    for (uint_fast64_t index = 0; index < this->getNumberOfSubformulas(); ++index) {
        if (index > 0) {
            out << ", ";
        }
        this->getSubformula(index).writeToStream(out);
    }
    out << ")";
    return out;
}
}  // namespace logic
}  // namespace storm
