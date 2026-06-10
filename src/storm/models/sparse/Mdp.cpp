#include "storm/models/sparse/Mdp.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/vector.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
Mdp<ValueType, RewardModelType>::Mdp(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
                                     std::unordered_map<std::string, RewardModelType> const& rewardModels, ModelType type)
    : Mdp<ValueType, RewardModelType>(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(transitionMatrix, stateLabeling, rewardModels),
                                      type) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Mdp<ValueType, RewardModelType>::Mdp(storm::storage::SparseMatrix<ValueType>&& transitionMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
                                     std::unordered_map<std::string, RewardModelType>&& rewardModels, ModelType type)
    : Mdp<ValueType, RewardModelType>(
          storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(std::move(transitionMatrix), std::move(stateLabeling), std::move(rewardModels)),
          type) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Mdp<ValueType, RewardModelType>::Mdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components, ModelType type)
    : NondeterministicModel<ValueType, RewardModelType>(type, components) {
    assert(type == storm::models::ModelType::Mdp || type == storm::models::ModelType::Pomdp);
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
Mdp<ValueType, RewardModelType>::Mdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components, ModelType type)
    : NondeterministicModel<ValueType, RewardModelType>(type, std::move(components)) {
    assert(type == storm::models::ModelType::Mdp || type == storm::models::ModelType::Pomdp);
    // Intentionally left empty
}

template class Mdp<double>;
template class Mdp<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class Mdp<storm::RationalNumber>;
template class Mdp<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class Mdp<storm::Interval>;
template class Mdp<storm::RationalInterval>;
template class Mdp<storm::RationalFunction>;
}  // namespace sparse
}  // namespace models
}  // namespace storm
