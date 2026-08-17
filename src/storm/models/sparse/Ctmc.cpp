#include "storm/models/sparse/Ctmc.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
Ctmc<ValueType, RewardModelType>::Ctmc(storm::storage::SparseMatrix<ValueType> const& rateMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
                                       std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Ctmc<ValueType, RewardModelType>(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(rateMatrix, stateLabeling, rewardModels, true)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Ctmc<ValueType, RewardModelType>::Ctmc(storm::storage::SparseMatrix<ValueType>&& rateMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
                                       std::unordered_map<std::string, RewardModelType>&& rewardModels)
    : Ctmc<ValueType, RewardModelType>(
          storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(std::move(rateMatrix), std::move(stateLabeling), std::move(rewardModels), true)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Ctmc<ValueType, RewardModelType>::Ctmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components)
    : DeterministicModel<ValueType, RewardModelType>(storm::models::ModelType::Ctmc, components) {
    if (components.exitRates) {
        exitRates = components.exitRates.get();
    } else {
        STORM_LOG_ASSERT(components.rateTransitions, "No rate information given for CTMC.");
        exitRates = createExitRateVector(this->getTransitionMatrix());
    }

    if (!components.rateTransitions) {
        this->getTransitionMatrix().scaleRowsInPlace(exitRates);
    }
}

template<typename ValueType, typename RewardModelType>
Ctmc<ValueType, RewardModelType>::Ctmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components)
    : DeterministicModel<ValueType, RewardModelType>(storm::models::ModelType::Ctmc, std::move(components)) {
    if (components.exitRates) {
        exitRates = std::move(components.exitRates.get());
    } else {
        STORM_LOG_ASSERT(components.rateTransitions, "No rate information given for CTMC.");
        exitRates = createExitRateVector(this->getTransitionMatrix());
    }

    if (!components.rateTransitions) {
        this->getTransitionMatrix().scaleRowsInPlace(exitRates);
    }
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> const& Ctmc<ValueType, RewardModelType>::getExitRateVector() const {
    return exitRates;
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType>& Ctmc<ValueType, RewardModelType>::getExitRateVector() {
    return exitRates;
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> Ctmc<ValueType, RewardModelType>::createExitRateVector(storm::storage::SparseMatrix<ValueType> const& rateMatrix) {
    std::vector<ValueType> exitRates(rateMatrix.getRowCount());
    for (uint_fast64_t row = 0; row < rateMatrix.getRowCount(); ++row) {
        exitRates[row] = rateMatrix.getRowSum(row);
    }
    return exitRates;
}

template<typename ValueType, typename RewardModelType>
void Ctmc<ValueType, RewardModelType>::reduceToStateBasedRewards() {
    for (auto& rewardModel : this->getRewardModels()) {
        rewardModel.second.reduceToStateBasedRewards(this->getTransitionMatrix(), true, &exitRates);
    }
}

template<typename ValueType, typename RewardModelType>
storm::storage::SparseMatrix<ValueType> Ctmc<ValueType, RewardModelType>::computeProbabilityMatrix() const {
    // Turn the rates into probabilities by scaling each row with the exit rate of the state.
    storm::storage::SparseMatrix<ValueType> result(this->getTransitionMatrix());
    for (uint_fast64_t row = 0; row < result.getRowCount(); ++row) {
        for (auto& entry : result.getRow(row)) {
            entry.setValue(entry.getValue() / exitRates[row]);
        }
    }
    return result;
}

template class Ctmc<double>;
template class Ctmc<storm::RationalNumber>;
template class Ctmc<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class Ctmc<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class Ctmc<storm::RationalFunction>;
template class Ctmc<storm::Interval>;
template class Ctmc<storm::RationalInterval>;
}  // namespace sparse
}  // namespace models
}  // namespace storm
