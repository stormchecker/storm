#include "storm-pomdp/analysis/FormulaInformation.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/utility/macros.h"

namespace storm {
namespace pomdp {
namespace analysis {

bool FormulaInformation::StateSet::empty() const {
    STORM_LOG_ASSERT(states.empty() == observations.empty(), "Inconsistent StateSet.");
    return observations.empty();
}

FormulaInformation::FormulaInformation() : type(Type::Unsupported) {
    // Intentionally left empty
}

FormulaInformation::FormulaInformation(Type const& type, storm::solver::OptimizationDirection const& dir, std::optional<std::string> const& rewardModelName)
    : type(type), optimizationDirection(dir), rewardModelName(rewardModelName) {
    STORM_LOG_ASSERT(
        !this->rewardModelName.has_value() || this->type == Type::NonNestedExpectedRewardFormula || this->type == Type::DiscountedTotalRewardFormula,
        "Got a reward model name for a non-reward formula.");
}

FormulaInformation::Type const& FormulaInformation::getType() const {
    return type;
}

bool FormulaInformation::isNonNestedReachabilityProbability() const {
    return type == Type::NonNestedReachabilityProbability;
}

bool FormulaInformation::isNonNestedExpectedRewardFormula() const {
    return type == Type::NonNestedExpectedRewardFormula;
}

bool FormulaInformation::isDiscountedTotalRewardFormula() const {
    return type == Type::DiscountedTotalRewardFormula;
}

bool FormulaInformation::isUnsupported() const {
    return type == Type::Unsupported;
}

bool FormulaInformation::isBounded() const {
    return rewardBounded;
}

typename FormulaInformation::StateSet const& FormulaInformation::getTargetStates() const {
    STORM_LOG_ASSERT(this->type == Type::NonNestedExpectedRewardFormula || this->type == Type::NonNestedReachabilityProbability,
                     "Target states requested for unexpected formula type.");
    return targetStates.value();
}

typename FormulaInformation::StateSet const& FormulaInformation::getSinkStates() const {
    STORM_LOG_ASSERT(this->type == Type::NonNestedReachabilityProbability, "Sink states requested for unexpected formula type.");
    return sinkStates.value();
}

std::string const& FormulaInformation::getRewardModelName() const {
    STORM_LOG_ASSERT(this->type == Type::NonNestedExpectedRewardFormula || this->type == Type::DiscountedTotalRewardFormula,
                     "Reward model requested for unexpected formula type.");
    return rewardModelName.value();
}

storm::solver::OptimizationDirection const& FormulaInformation::getOptimizationDirection() const {
    return optimizationDirection;
}

std::vector<storm::logic::TimeBoundReference> const& FormulaInformation::getRewardBoundReferences() const {
    return rewardBoundReferences;
}

bool FormulaInformation::minimize() const {
    return storm::solver::minimize(optimizationDirection);
}

bool FormulaInformation::maximize() const {
    return storm::solver::maximize(optimizationDirection);
}
void FormulaInformation::setRewardBounded(bool newValue) {
    rewardBounded = newValue;
}

template<typename PomdpType>
FormulaInformation::StateSet getStateSet(PomdpType const& pomdp, storm::storage::BitVector&& inputStates) {
    FormulaInformation::StateSet result;
    result.states = std::move(inputStates);
    for (auto const state : result.states) {
        result.observations.insert(pomdp.getObservation(state));
    }
    // check if this set is observation-closed, i.e., whether there is a state outside of this set with one of the observations collected above
    result.observationClosed = true;
    for (uint64_t state = result.states.getNextUnsetIndex(0); state < result.states.size(); state = result.states.getNextUnsetIndex(state + 1)) {
        if (result.observations.count(pomdp.getObservation(state)) > 0) {
            result.observationClosed = false;
            break;
        }
    }
    return result;
}

template<typename PomdpType>
void FormulaInformation::updateTargetStates(PomdpType const& pomdp, storm::storage::BitVector&& newTargetStates) {
    STORM_LOG_ASSERT(this->type == Type::NonNestedExpectedRewardFormula || this->type == Type::NonNestedReachabilityProbability,
                     "Target states updated for unexpected formula type.");
    targetStates = getStateSet(pomdp, std::move(newTargetStates));
}

template<typename PomdpType>
void FormulaInformation::updateSinkStates(PomdpType const& pomdp, storm::storage::BitVector&& newSinkStates) {
    STORM_LOG_ASSERT(this->type == Type::NonNestedReachabilityProbability, "Sink states requested for unexpected formula type.");
    sinkStates = getStateSet(pomdp, std::move(newSinkStates));
}
void FormulaInformation::setRewardBoundReferences(std::vector<logic::TimeBoundReference>& newRewardBoundReferences) {
    STORM_LOG_ASSERT(this->type == Type::NonNestedReachabilityProbability && this->isBounded(), "RewardBoundReference requested for unexpected formula type.");
    rewardBoundReferences = newRewardBoundReferences;
}

template<typename PomdpType>
storm::storage::BitVector getStates(storm::logic::Formula const& propositionalFormula, bool formulaInverted, PomdpType const& pomdp) {
    storm::modelchecker::SparsePropositionalModelChecker<PomdpType> mc(pomdp);
    auto checkResult = mc.check(propositionalFormula);
    storm::storage::BitVector resultBitVector(checkResult->template asExplicitQualitativeCheckResult<typename PomdpType::ValueType>().getTruthValuesVector());
    if (formulaInverted) {
        resultBitVector.complement();
    }
    return resultBitVector;
}

template<typename PomdpType>
FormulaInformation getFormulaInformation(PomdpType const& pomdp, storm::logic::ProbabilityOperatorFormula const& formula) {
    STORM_LOG_THROW(formula.hasOptimalityType(), storm::exceptions::InvalidPropertyException,
                    "The property does not specify an optimization direction (min/max).");
    STORM_LOG_WARN_COND(!formula.hasBound(), "The probability threshold for the given property will be ignored.");
    auto const& subformula = formula.getSubformula();
    bool bounded = false;
    std::vector<storm::logic::TimeBoundReference> rewardBoundReferences;

    std::shared_ptr<storm::logic::Formula const> targetStatesFormula, constraintsStatesFormula;
    if (subformula.isEventuallyFormula()) {
        targetStatesFormula = subformula.asEventuallyFormula().getSubformula().asSharedPointer();
        constraintsStatesFormula = storm::logic::Formula::getTrueFormula()->asSharedPointer();
    } else if (subformula.isUntilFormula()) {
        storm::logic::UntilFormula const& untilFormula = subformula.asUntilFormula();
        targetStatesFormula = untilFormula.getRightSubformula().asSharedPointer();
        constraintsStatesFormula = untilFormula.getLeftSubformula().asSharedPointer();
    } else if (subformula.isBoundedUntilFormula()) {
        storm::logic::BoundedUntilFormula const& boundedUntilFormula = subformula.asBoundedUntilFormula();
        targetStatesFormula = boundedUntilFormula.getRightSubformula().asSharedPointer();
        constraintsStatesFormula = boundedUntilFormula.getLeftSubformula().asSharedPointer();
        bounded = true;
        for (uint64_t i = 0; i < boundedUntilFormula.getDimension(); ++i) {
            STORM_LOG_ASSERT(boundedUntilFormula.getTimeBoundReference(i).isRewardBound(), "Expected a reward bound reference.");
            rewardBoundReferences.push_back(boundedUntilFormula.getTimeBoundReference(i));
        }
    }
    if (targetStatesFormula && targetStatesFormula->isInFragment(storm::logic::propositional()) && constraintsStatesFormula &&
        constraintsStatesFormula->isInFragment(storm::logic::propositional())) {
        FormulaInformation result(FormulaInformation::Type::NonNestedReachabilityProbability, formula.getOptimalityType());
        result.updateTargetStates(pomdp, getStates(*targetStatesFormula, false, pomdp));
        result.updateSinkStates(pomdp, getStates(*constraintsStatesFormula, true, pomdp));
        result.setRewardBounded(bounded);
        if (bounded) {
            result.setRewardBoundReferences(rewardBoundReferences);
        }
        return result;
    }
    return FormulaInformation();
}

template<typename PomdpType>
FormulaInformation getFormulaInformation(PomdpType const& pomdp, storm::logic::RewardOperatorFormula const& formula) {
    STORM_LOG_THROW(formula.hasOptimalityType(), storm::exceptions::InvalidPropertyException,
                    "The property does not specify an optimization direction (min/max).");
    STORM_LOG_WARN_COND(!formula.hasBound(), "The reward threshold for the given property will be ignored.");
    std::string rewardModelName = "";
    if (formula.hasRewardModelName()) {
        rewardModelName = formula.getRewardModelName();
        STORM_LOG_THROW(pomdp.hasRewardModel(rewardModelName), storm::exceptions::InvalidPropertyException,
                        "Selected reward model with name '" << rewardModelName << "' does not exist.");
    } else {
        STORM_LOG_THROW(pomdp.hasUniqueRewardModel(), storm::exceptions::InvalidPropertyException,
                        "Reward operator formula does not specify a reward model and the reward model is not unique.");
        rewardModelName = pomdp.getUniqueRewardModelName();
    }
    auto const& subformula = formula.getSubformula();
    if (subformula.isDiscountedTotalRewardFormula()) {
        FormulaInformation result(FormulaInformation::Type::DiscountedTotalRewardFormula, formula.getOptimalityType(), rewardModelName);
        return result;
    }
    std::shared_ptr<storm::logic::Formula const> targetStatesFormula;
    if (subformula.isEventuallyFormula()) {
        targetStatesFormula = subformula.asEventuallyFormula().getSubformula().asSharedPointer();
    }
    if (targetStatesFormula && targetStatesFormula->isInFragment(storm::logic::propositional())) {
        FormulaInformation result(FormulaInformation::Type::NonNestedExpectedRewardFormula, formula.getOptimalityType(), rewardModelName);
        result.updateTargetStates(pomdp, getStates(*targetStatesFormula, false, pomdp));
        return result;
    }
    return FormulaInformation();
}

template<typename PomdpType>
FormulaInformation getFormulaInformation(PomdpType const& pomdp, storm::logic::Formula const& formula) {
    if (formula.isProbabilityOperatorFormula()) {
        return getFormulaInformation(pomdp, formula.asProbabilityOperatorFormula());
    } else if (formula.isRewardOperatorFormula()) {
        return getFormulaInformation(pomdp, formula.asRewardOperatorFormula());
    }
    return FormulaInformation();
}

template void FormulaInformation::updateTargetStates<storm::models::sparse::Pomdp<double>>(storm::models::sparse::Pomdp<double> const& pomdp,
                                                                                           storm::storage::BitVector&& newTargetStates);
template void FormulaInformation::updateSinkStates<storm::models::sparse::Pomdp<double>>(storm::models::sparse::Pomdp<double> const& pomdp,
                                                                                         storm::storage::BitVector&& newSinkStates);
template FormulaInformation getFormulaInformation<storm::models::sparse::Pomdp<double>>(storm::models::sparse::Pomdp<double> const& pomdp,
                                                                                        storm::logic::Formula const& formula);
template void FormulaInformation::updateTargetStates<storm::models::sparse::Pomdp<storm::RationalNumber>>(
    storm::models::sparse::Pomdp<storm::RationalNumber> const& pomdp, storm::storage::BitVector&& newTargetStates);
template void FormulaInformation::updateSinkStates<storm::models::sparse::Pomdp<storm::RationalNumber>>(
    storm::models::sparse::Pomdp<storm::RationalNumber> const& pomdp, storm::storage::BitVector&& newSinkStates);
template FormulaInformation getFormulaInformation<storm::models::sparse::Pomdp<storm::RationalNumber>>(
    storm::models::sparse::Pomdp<storm::RationalNumber> const& pomdp, storm::logic::Formula const& formula);

}  // namespace analysis
}  // namespace pomdp
}  // namespace storm
