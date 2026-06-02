#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/multiobjective/pcaa/RewardBoundedMdpPcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/pcaa/StandardMaPcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/pcaa/StandardMdpPcaaWeightVectorChecker.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<typename ModelType>
PcaaWeightVectorChecker<ModelType>::PcaaWeightVectorChecker(std::vector<Objective<ValueType>> const& objectives) : objectives(objectives) {
    // Intentionally left empty
}

template<typename ModelType>
void PcaaWeightVectorChecker<ModelType>::setWeightedPrecision(ValueType const& value) {
    weightedPrecision = value;
}

template<typename ModelType>
typename PcaaWeightVectorChecker<ModelType>::ValueType const& PcaaWeightVectorChecker<ModelType>::getWeightedPrecision() const {
    STORM_LOG_THROW(weightedPrecision.has_value(), storm::exceptions::InvalidOperationException, "The weighted precision has not been set.");
    return weightedPrecision.value();
}

template<typename ModelType>
bool PcaaWeightVectorChecker<ModelType>::smallPrecisionsAreChallenging() const {
    return false;
}

template<typename ModelType>
storm::storage::Scheduler<typename PcaaWeightVectorChecker<ModelType>::ValueType> PcaaWeightVectorChecker<ModelType>::computeScheduler() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Scheduler generation is not supported in this setting.");
}

template<class SparseModelType>
boost::optional<typename SparseModelType::ValueType> PcaaWeightVectorChecker<SparseModelType>::computeWeightedResultBound(
    bool lower, std::vector<ValueType> const& weightVector, storm::storage::BitVector const& objectiveFilter) const {
    ValueType result = storm::utility::zero<ValueType>();
    for (auto objIndex : objectiveFilter) {
        // get the actual weight for this objective, i.e., negate it if this is a minimizing objective
        auto const weight = storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()) ? -weightVector[objIndex] : weightVector[objIndex];
        if (storm::utility::isZero(weight)) {
            continue;  // this objective is not relevant for the weighted sum, so we can skip it
        } else {
            // Choose the requested, relevant bound (lower or upper). Invert the choice for negative weights.
            boost::optional<ValueType> const& objBound = (lower == (weight > storm::utility::zero<ValueType>())) ? this->objectives[objIndex].lowerResultBound
                                                                                                                 : this->objectives[objIndex].upperResultBound;
            if (objBound) {
                result += objBound.get() * weight;
            } else {
                // If there is an objective without the corresponding bound we can not give guarantees for the weighted sum
                return boost::none;
            }
        }
    }
    return result;
}

template<typename ModelType>
std::unique_ptr<PcaaWeightVectorChecker<ModelType>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<ModelType> const& preprocessorResult) {
    if constexpr (std::is_same_v<ModelType, storm::models::sparse::Mdp<typename ModelType::ValueType>>) {
        if (preprocessorResult.containsRewardBoundedObjective()) {
            return std::make_unique<RewardBoundedMdpPcaaWeightVectorChecker<ModelType>>(preprocessorResult);
        } else {
            return std::make_unique<StandardMdpPcaaWeightVectorChecker<ModelType>>(preprocessorResult);
        }
    } else {
        static_assert(std::is_same_v<ModelType, storm::models::sparse::MarkovAutomaton<typename ModelType::ValueType>>);
        STORM_LOG_THROW(!preprocessorResult.containsRewardBoundedObjective(), storm::exceptions::NotSupportedException,
                        "Reward-bounded objectives are not supported for multi-objective Markov automata.");
        return std::make_unique<StandardMaPcaaWeightVectorChecker<ModelType>>(preprocessorResult);
    }
}

template class PcaaWeightVectorChecker<storm::models::sparse::Mdp<double>>;
template class PcaaWeightVectorChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class PcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>;
template class PcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;

template std::unique_ptr<PcaaWeightVectorChecker<storm::models::sparse::Mdp<double>>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<storm::models::sparse::Mdp<double>> const& preprocessorResult);
template std::unique_ptr<PcaaWeightVectorChecker<storm::models::sparse::Mdp<RationalNumber>>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<storm::models::sparse::Mdp<RationalNumber>> const& preprocessorResult);
template std::unique_ptr<PcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<storm::models::sparse::MarkovAutomaton<double>> const& preprocessorResult);
template std::unique_ptr<PcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<RationalNumber>>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<storm::models::sparse::MarkovAutomaton<RationalNumber>> const& preprocessorResult);

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
