#include "storm/models/sparse/Dtmc.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
Dtmc<ValueType, RewardModelType>::Dtmc(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                       storm::models::sparse::StateLabeling const& stateLabeling,
                                       std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Dtmc<ValueType, RewardModelType>(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(transitionMatrix, stateLabeling, rewardModels)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Dtmc<ValueType, RewardModelType>::Dtmc(storm::storage::SparseMatrix<ValueType>&& transitionMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
                                       std::unordered_map<std::string, RewardModelType>&& rewardModels)
    : Dtmc<ValueType, RewardModelType>(
          storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(std::move(transitionMatrix), std::move(stateLabeling), std::move(rewardModels))) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Dtmc<ValueType, RewardModelType>::Dtmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components)
    : DeterministicModel<ValueType, RewardModelType>(storm::models::ModelType::Dtmc, components) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Dtmc<ValueType, RewardModelType>::Dtmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components)
    : DeterministicModel<ValueType, RewardModelType>(storm::models::ModelType::Dtmc, std::move(components)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
void Dtmc<ValueType, RewardModelType>::reduceToStateBasedRewards() {
    for (auto& rewardModel : this->getRewardModels()) {
        rewardModel.second.reduceToStateBasedRewards(this->getTransitionMatrix(), true);
    }
}

template class Dtmc<double>;
template class Dtmc<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class Dtmc<storm::RationalNumber>;
template class Dtmc<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class Dtmc<storm::Interval>;
template class Dtmc<storm::RationalInterval>;
template class Dtmc<storm::RationalFunction>;
}  // namespace sparse
}  // namespace models
}  // namespace storm
