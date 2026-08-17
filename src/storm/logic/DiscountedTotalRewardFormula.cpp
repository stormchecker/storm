#include "storm/logic/DiscountedTotalRewardFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
DiscountedTotalRewardFormula::DiscountedTotalRewardFormula(storm::expressions::Expression const discountFactor,
                                                           boost::optional<RewardAccumulation> rewardAccumulation)
    : TotalRewardFormula(rewardAccumulation), discountFactor(discountFactor) {}

bool DiscountedTotalRewardFormula::isDiscountedTotalRewardFormula() const {
    return true;
}

bool DiscountedTotalRewardFormula::isTotalRewardFormula() const {
    return false;
}

std::ostream& DiscountedTotalRewardFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "Cdiscount=";
    out << discountFactor.toString();
    if (hasRewardAccumulation()) {
        out << "[" << getRewardAccumulation() << "]";
    }
    return out;
}

void DiscountedTotalRewardFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    this->getDiscountFactor().gatherVariables(usedVariables);
}

storm::expressions::Expression const& DiscountedTotalRewardFormula::getDiscountFactor() const {
    return discountFactor;
}

template<>
double DiscountedTotalRewardFormula::getDiscountFactor() const {
    checkNoVariablesInDiscountFactor(discountFactor);
    double value = discountFactor.evaluateAsDouble();
    STORM_LOG_THROW(value > 0 && value < 1, storm::exceptions::InvalidPropertyException, "Discount factor must be strictly between 0 and 1.");
    return value;
}

template<>
storm::RationalNumber DiscountedTotalRewardFormula::getDiscountFactor() const {
    checkNoVariablesInDiscountFactor(discountFactor);
    storm::RationalNumber value = discountFactor.evaluateAsRational();
    STORM_LOG_THROW(value > 0 && value < 1, storm::exceptions::InvalidPropertyException, "Discount factor must be strictly between 0 and 1.");
    return value;
}

void DiscountedTotalRewardFormula::checkNoVariablesInDiscountFactor(storm::expressions::Expression const& factor) {
    STORM_LOG_THROW(!factor.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate discount factor '" << factor << "' as it contains undefined constants.");
}

boost::any DiscountedTotalRewardFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

}  // namespace logic
}  // namespace storm
