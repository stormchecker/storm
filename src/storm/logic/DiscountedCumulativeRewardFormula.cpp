#include "storm/logic/DiscountedCumulativeRewardFormula.h"

#include <boost/any.hpp>
#include <ostream>
#include <utility>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
DiscountedCumulativeRewardFormula::DiscountedCumulativeRewardFormula(storm::expressions::Expression const& discountFactor, TimeBound const& bound,
                                                                     TimeBoundReference const& timeBoundReference,
                                                                     boost::optional<RewardAccumulation> rewardAccumulation)
    : CumulativeRewardFormula(bound, timeBoundReference, std::move(rewardAccumulation)), discountFactor(discountFactor) {
    // Intentionally left empty.
}

DiscountedCumulativeRewardFormula::DiscountedCumulativeRewardFormula(storm::expressions::Expression const& discountFactor, std::vector<TimeBound> const& bounds,
                                                                     std::vector<TimeBoundReference> const& timeBoundReferences,
                                                                     boost::optional<RewardAccumulation> rewardAccumulation)
    : CumulativeRewardFormula(bounds, timeBoundReferences, std::move(rewardAccumulation)), discountFactor(discountFactor) {
    // Intentionally left empty.
}

bool DiscountedCumulativeRewardFormula::isDiscountedCumulativeRewardFormula() const {
    return true;
}

bool DiscountedCumulativeRewardFormula::isCumulativeRewardFormula() const {
    return false;
}

void DiscountedCumulativeRewardFormula::checkNoVariablesInDiscountFactor(storm::expressions::Expression const& factor) {
    STORM_LOG_THROW(!factor.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate discount factor '" << factor << "' as it contains undefined constants.");
}

void DiscountedCumulativeRewardFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    for (unsigned i = 0; i < this->getDimension(); ++i) {
        this->getBound(i).gatherVariables(usedVariables);
    }
    this->getDiscountFactor().gatherVariables(usedVariables);
}

storm::expressions::Expression const& DiscountedCumulativeRewardFormula::getDiscountFactor() const {
    return discountFactor;
}

template<>
double DiscountedCumulativeRewardFormula::getDiscountFactor() const {
    checkNoVariablesInDiscountFactor(discountFactor);
    double value = discountFactor.evaluateAsDouble();
    STORM_LOG_THROW(value > 0 && value < 1, storm::exceptions::InvalidPropertyException, "Discount factor must be strictly between 0 and 1.");
    return value;
}

template<>
storm::RationalNumber DiscountedCumulativeRewardFormula::getDiscountFactor() const {
    checkNoVariablesInDiscountFactor(discountFactor);
    storm::RationalNumber value = discountFactor.evaluateAsRational();
    STORM_LOG_THROW(value > 0 && value < 1, storm::exceptions::InvalidPropertyException, "Discount factor must be strictly between 0 and 1.");
    return value;
}

std::ostream& DiscountedCumulativeRewardFormula::writeToStream(std::ostream& out, bool /*allowParentheses*/) const {
    // No parentheses necessary
    out << "C";
    if (hasRewardAccumulation()) {
        out << "[" << getRewardAccumulation() << "]";
    }
    if (this->isMultiDimensional()) {
        out << "^{";
    }
    for (unsigned i = 0; i < this->getDimension(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        if (this->getTimeBoundReference(i).isRewardBound()) {
            out << "rew";
            if (this->getTimeBoundReference(i).hasRewardAccumulation()) {
                out << "[" << this->getTimeBoundReference(i).getRewardAccumulation() << "]";
            }
            out << "{\"" << this->getTimeBoundReference(i).getRewardName() << "\"}";
        } else if (this->getTimeBoundReference(i).isStepBound()) {
            out << "steps";
            //} else if (this->getTimeBoundReference(i).isStepBound())
            //  Note: the 'time' keyword is optional.
            //    out << "time";
        }
        if (this->isBoundStrict(i)) {
            out << "<";
        } else {
            out << "<=";
        }
        out << this->getBound(i);
    }
    if (this->isMultiDimensional()) {
        out << "}";
    }
    out << "discount=";
    out << discountFactor;
    return out;
}

boost::any DiscountedCumulativeRewardFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

}  // namespace logic
}  // namespace storm
