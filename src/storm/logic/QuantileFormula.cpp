#include "storm/logic/QuantileFormula.h"
#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

QuantileFormula::QuantileFormula(std::vector<storm::expressions::Variable> const& boundVariables, std::shared_ptr<Formula const> subformula)
    : boundVariables(boundVariables), subformula(subformula) {
    STORM_LOG_THROW(!boundVariables.empty(), storm::exceptions::InvalidArgumentException, "Quantile formula without bound variables are invalid.");
}

QuantileFormula::~QuantileFormula() {
    // Intentionally left empty
}

bool QuantileFormula::isQuantileFormula() const {
    return true;
}

bool QuantileFormula::hasQuantitativeResult() const {
    return true;
}

bool QuantileFormula::hasNumericalResult() const {
    return !isMultiDimensional();
}

bool QuantileFormula::hasMultiDimensionalResult() const {
    return isMultiDimensional();
}

Formula const& QuantileFormula::getSubformula() const {
    return *subformula;
}

uint64_t QuantileFormula::getDimension() const {
    return boundVariables.size();
}

bool QuantileFormula::isMultiDimensional() const {
    return getDimension() > 1;
}

storm::expressions::Variable const& QuantileFormula::getBoundVariable() const {
    STORM_LOG_THROW(boundVariables.size() == 1, storm::exceptions::InvalidArgumentException,
                    "Requested unique bound variables. However, there are multiple bound variables defined.");
    return boundVariables.front();
}

storm::expressions::Variable const& QuantileFormula::getBoundVariable(uint64_t index) const {
    STORM_LOG_THROW(index < boundVariables.size(), storm::exceptions::InvalidArgumentException,
                    "Requested bound variable with index" << index << ". However, there are only " << boundVariables.size() << " bound variables.");
    return boundVariables[index];
}

std::vector<storm::expressions::Variable> const& QuantileFormula::getBoundVariables() const {
    return boundVariables;
}

boost::any QuantileFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void QuantileFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    subformula->gatherAtomicExpressionFormulas(atomicExpressionFormulas);
}

void QuantileFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    subformula->gatherAtomicLabelFormulas(atomicLabelFormulas);
}

void QuantileFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    subformula->gatherReferencedRewardModels(referencedRewardModels);
}

std::ostream& QuantileFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "quantile(";
    for (auto const& bv : boundVariables) {
        out << bv.getName() << ", ";
    }
    subformula->writeToStream(out);
    out << ")";
    return out;
}
}  // namespace logic
}  // namespace storm
