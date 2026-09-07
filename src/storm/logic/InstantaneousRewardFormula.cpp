#include "InstantaneousRewardFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
InstantaneousRewardFormula::InstantaneousRewardFormula(storm::expressions::Expression const& bound, TimeBoundType const& timeBoundType)
    : timeBoundType(timeBoundType), bound(bound) {
    // Intentionally left empty.
}

bool InstantaneousRewardFormula::isInstantaneousRewardFormula() const {
    return true;
}

bool InstantaneousRewardFormula::isRewardPathFormula() const {
    return true;
}

boost::any InstantaneousRewardFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

TimeBoundType const& InstantaneousRewardFormula::getTimeBoundType() const {
    return timeBoundType;
}

bool InstantaneousRewardFormula::isStepBounded() const {
    return timeBoundType == TimeBoundType::Steps;
}

bool InstantaneousRewardFormula::isTimeBounded() const {
    return timeBoundType == TimeBoundType::Time;
}

bool InstantaneousRewardFormula::hasIntegerBound() const {
    return bound.hasIntegerType();
}

storm::expressions::Expression const& InstantaneousRewardFormula::getBound() const {
    return bound;
}

template<>
double InstantaneousRewardFormula::getBound() const {
    checkNoVariablesInBound(bound);
    double value = bound.evaluateAsDouble();
    STORM_LOG_THROW(value >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return value;
}

template<>
uint64_t InstantaneousRewardFormula::getBound() const {
    checkNoVariablesInBound(bound);
    int64_t value = bound.evaluateAsInt();
    STORM_LOG_THROW(value >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return value;
}

void InstantaneousRewardFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getBound().gatherVariables(usedVariables);
}

void InstantaneousRewardFormula::checkNoVariablesInBound(storm::expressions::Expression const& bound) {
    STORM_LOG_THROW(!bound.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate time-instant '" << bound << "' as it contains undefined constants.");
}

std::ostream& InstantaneousRewardFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary.
    out << "I=" << this->getBound();
    return out;
}
}  // namespace logic
}  // namespace storm
