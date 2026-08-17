
#include "storm/logic/CumulativeRewardFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
CumulativeRewardFormula::CumulativeRewardFormula(TimeBound const& bound, TimeBoundReference const& timeBoundReference,
                                                 boost::optional<RewardAccumulation> rewardAccumulation)
    : timeBoundReferences({timeBoundReference}), bounds({bound}), rewardAccumulation(rewardAccumulation) {
    // Intentionally left empty.
}

CumulativeRewardFormula::CumulativeRewardFormula(std::vector<TimeBound> const& bounds, std::vector<TimeBoundReference> const& timeBoundReferences,
                                                 boost::optional<RewardAccumulation> rewardAccumulation)
    : timeBoundReferences(timeBoundReferences), bounds(bounds), rewardAccumulation(rewardAccumulation) {
    STORM_LOG_ASSERT(this->timeBoundReferences.size() == this->bounds.size(), "Number of time bounds and number of time bound references does not match.");
    STORM_LOG_ASSERT(!this->timeBoundReferences.empty(), "No time bounds given.");
}

bool CumulativeRewardFormula::isCumulativeRewardFormula() const {
    return true;
}

bool CumulativeRewardFormula::isRewardPathFormula() const {
    return true;
}

bool CumulativeRewardFormula::isMultiDimensional() const {
    return getDimension() > 1;
}

unsigned CumulativeRewardFormula::getDimension() const {
    return timeBoundReferences.size();
}

boost::any CumulativeRewardFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void CumulativeRewardFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    for (unsigned i = 0; i < this->getDimension(); ++i) {
        if (getTimeBoundReference(i).isRewardBound()) {
            referencedRewardModels.insert(this->getTimeBoundReference(i).getRewardName());
        }
    }
}

void CumulativeRewardFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    for (unsigned i = 0; i < this->getDimension(); ++i) {
        this->getBound(i).gatherVariables(usedVariables);
    }
}

TimeBoundReference const& CumulativeRewardFormula::getTimeBoundReference() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    return getTimeBoundReference(0);
}

TimeBoundReference const& CumulativeRewardFormula::getTimeBoundReference(unsigned i) const {
    return timeBoundReferences.at(i);
}

bool CumulativeRewardFormula::isBoundStrict() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    return isBoundStrict(0);
}

bool CumulativeRewardFormula::isBoundStrict(unsigned i) const {
    return bounds.at(i).isStrict();
}

bool CumulativeRewardFormula::hasIntegerBound() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    return hasIntegerBound(0);
}

bool CumulativeRewardFormula::hasIntegerBound(unsigned i) const {
    return bounds.at(i).getBound().hasIntegerType();
}

storm::expressions::Expression const& CumulativeRewardFormula::getBound() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    return getBound(0);
}

storm::expressions::Expression const& CumulativeRewardFormula::getBound(unsigned i) const {
    return bounds.at(i).getBound();
}

template<typename ValueType>
ValueType CumulativeRewardFormula::getBound() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    return getBound<ValueType>(0);
}

template<>
double CumulativeRewardFormula::getBound(unsigned i) const {
    checkNoVariablesInBound(bounds.at(i).getBound());
    double value = bounds.at(i).getBound().evaluateAsDouble();
    return value;
}

template<>
storm::RationalNumber CumulativeRewardFormula::getBound(unsigned i) const {
    checkNoVariablesInBound(bounds.at(i).getBound());
    storm::RationalNumber value = bounds.at(i).getBound().evaluateAsRational();
    return value;
}

template<>
uint64_t CumulativeRewardFormula::getBound(unsigned i) const {
    checkNoVariablesInBound(bounds.at(i).getBound());
    uint64_t value = bounds.at(i).getBound().evaluateAsInt();
    return value;
}

template<>
double CumulativeRewardFormula::getNonStrictBound() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    double bound = getBound<double>();
    STORM_LOG_THROW(bound > 0, storm::exceptions::InvalidPropertyException, "Cannot retrieve non-strict bound from strict zero-bound.");
    return bound;
}

template<>
uint64_t CumulativeRewardFormula::getNonStrictBound() const {
    STORM_LOG_ASSERT(!isMultiDimensional(), "Cumulative Reward Formula is multi-dimensional.");
    int_fast64_t bound = getBound<uint64_t>();
    if (isBoundStrict()) {
        STORM_LOG_THROW(bound > 0, storm::exceptions::InvalidPropertyException, "Cannot retrieve non-strict bound from strict zero-bound.");
        return bound - 1;
    } else {
        return bound;
    }
}

std::vector<TimeBound> const& CumulativeRewardFormula::getBounds() const {
    return bounds;
}

void CumulativeRewardFormula::checkNoVariablesInBound(storm::expressions::Expression const& bound) {
    STORM_LOG_THROW(!bound.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate time-bound '" << bound << "' as it contains undefined constants.");
}

bool CumulativeRewardFormula::hasRewardAccumulation() const {
    return rewardAccumulation.is_initialized();
}

RewardAccumulation const& CumulativeRewardFormula::getRewardAccumulation() const {
    return rewardAccumulation.get();
}

std::shared_ptr<CumulativeRewardFormula const> CumulativeRewardFormula::stripRewardAccumulation() const {
    return std::make_shared<CumulativeRewardFormula const>(bounds, timeBoundReferences);
}

std::shared_ptr<CumulativeRewardFormula const> CumulativeRewardFormula::restrictToDimension(unsigned i) const {
    return std::make_shared<CumulativeRewardFormula const>(bounds.at(i), getTimeBoundReference(i), rewardAccumulation);
}

std::ostream& CumulativeRewardFormula::writeToStream(std::ostream& out, bool /*allowParentheses*/) const {
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
    return out;
}

template uint64_t CumulativeRewardFormula::getBound<uint64_t>() const;
template double CumulativeRewardFormula::getBound<double>() const;
template storm::RationalNumber CumulativeRewardFormula::getBound<storm::RationalNumber>() const;

}  // namespace logic
}  // namespace storm
