#include "storm/logic/OperatorFormula.h"

#include <ostream>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/logic/Bound.h"

namespace storm {
namespace logic {
OperatorInformation::OperatorInformation(boost::optional<storm::solver::OptimizationDirection> const& optimizationDirection,
                                         boost::optional<Bound> const& bound)
    : optimalityType(optimizationDirection), bound(bound) {
    // Intentionally left empty.
}

OperatorFormula::OperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation)
    : UnaryStateFormula(subformula), operatorInformation(operatorInformation) {
    // Intentionally left empty.
}

bool OperatorFormula::hasBound() const {
    return static_cast<bool>(operatorInformation.bound);
}

ComparisonType OperatorFormula::getComparisonType() const {
    STORM_LOG_ASSERT(operatorInformation.bound.is_initialized(), "Cannot get Formula comparison type (has no bound?)");
    return operatorInformation.bound.get().comparisonType;
}

void OperatorFormula::setComparisonType(ComparisonType newComparisonType) {
    operatorInformation.bound.get().comparisonType = newComparisonType;
}

storm::expressions::Expression const& OperatorFormula::getThreshold() const {
    return operatorInformation.bound.get().threshold;
}

template<>
double OperatorFormula::getThresholdAs() const {
    STORM_LOG_THROW(!operatorInformation.bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate threshold '" << operatorInformation.bound.get().threshold << "' as it contains undefined constants.");
    return operatorInformation.bound.get().threshold.evaluateAsDouble();
}

template<>
storm::RationalNumber OperatorFormula::getThresholdAs() const {
    STORM_LOG_THROW(!operatorInformation.bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate threshold '" << operatorInformation.bound.get().threshold << "' as it contains undefined constants.");
    return operatorInformation.bound.get().threshold.evaluateAsRational();
}

template<>
storm::RationalFunction OperatorFormula::getThresholdAs() const {
    STORM_LOG_THROW(!operatorInformation.bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate threshold '" << operatorInformation.bound.get().threshold << "' as it contains undefined constants.");
    return storm::utility::convertNumber<storm::RationalFunction>(operatorInformation.bound.get().threshold.evaluateAsRational());
}

template<>
storm::Interval OperatorFormula::getThresholdAs() const {
    STORM_LOG_THROW(!operatorInformation.bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate threshold '" << operatorInformation.bound.get().threshold << "' as it contains undefined constants.");
    return storm::utility::convertNumber<storm::Interval>(operatorInformation.bound.get().threshold.evaluateAsRational());
}

template<>
storm::RationalInterval OperatorFormula::getThresholdAs() const {
    STORM_LOG_THROW(!operatorInformation.bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate threshold '" << operatorInformation.bound.get().threshold << "' as it contains undefined constants.");
    return storm::utility::convertNumber<storm::RationalInterval>(operatorInformation.bound.get().threshold.evaluateAsRational());
}

void OperatorFormula::setThreshold(storm::expressions::Expression const& newThreshold) {
    operatorInformation.bound.get().threshold = newThreshold;
}

Bound const& OperatorFormula::getBound() const {
    return operatorInformation.bound.get();
}

void OperatorFormula::setBound(Bound const& newBound) {
    operatorInformation.bound = newBound;
}

void OperatorFormula::removeBound() {
    operatorInformation.bound = boost::none;
}

bool OperatorFormula::hasOptimalityType() const {
    return static_cast<bool>(operatorInformation.optimalityType);
}

OptimizationDirection const& OperatorFormula::getOptimalityType() const {
    return operatorInformation.optimalityType.get();
}

void OperatorFormula::setOptimalityType(storm::solver::OptimizationDirection newOptimalityType) {
    operatorInformation.optimalityType = newOptimalityType;
}

void OperatorFormula::removeOptimalityType() {
    operatorInformation.optimalityType = boost::none;
}

bool OperatorFormula::isOperatorFormula() const {
    return true;
}

OperatorInformation const& OperatorFormula::getOperatorInformation() const {
    return operatorInformation;
}

bool OperatorFormula::hasQualitativeResult() const {
    return this->hasBound();
}

bool OperatorFormula::hasQuantitativeResult() const {
    return !this->hasBound();
}

void OperatorFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    UnaryStateFormula::gatherUsedVariables(usedVariables);
    if (this->hasBound()) {
        this->getThreshold().gatherVariables(usedVariables);
    }
}

std::ostream& OperatorFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    if (hasOptimalityType()) {
        out << (getOptimalityType() == OptimizationDirection::Minimize ? "min" : "max");
    }
    if (hasBound()) {
        out << getBound();
    } else {
        out << "=?";
    }
    out << " [";
    this->getSubformula().writeToStream(out);
    out << "]";
    return out;
}
}  // namespace logic
}  // namespace storm
