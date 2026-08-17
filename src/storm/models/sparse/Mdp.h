#pragma once

#include "storm/models/sparse/NondeterministicModel.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a (discrete-time) Markov decision process.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class Mdp : public NondeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Mdp(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
        std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>(),
        ModelType type = ModelType::Mdp);

    /*!
     * Constructs a model by moving the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Mdp(storm::storage::SparseMatrix<ValueType>&& transitionMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
        std::unordered_map<std::string, RewardModelType>&& rewardModels = std::unordered_map<std::string, RewardModelType>(), ModelType type = ModelType::Mdp);

    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    Mdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components, ModelType type = ModelType::Mdp);
    Mdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components, ModelType type = ModelType::Mdp);

    Mdp(Mdp<ValueType, RewardModelType> const& other) = default;
    Mdp& operator=(Mdp<ValueType, RewardModelType> const& other) = default;

    Mdp(Mdp<ValueType, RewardModelType>&& other) = default;
    Mdp& operator=(Mdp<ValueType, RewardModelType>&& other) = default;

    virtual ~Mdp() = default;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
