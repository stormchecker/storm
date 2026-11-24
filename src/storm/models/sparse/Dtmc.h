#pragma once

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/models/sparse/DeterministicModel.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a discrete-time Markov chain.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class Dtmc : public DeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Dtmc(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model by moving the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Dtmc(storm::storage::SparseMatrix<ValueType>&& transitionMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
         std::unordered_map<std::string, RewardModelType>&& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    Dtmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    Dtmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    Dtmc(Dtmc<ValueType, RewardModelType> const& dtmc) = default;
    Dtmc& operator=(Dtmc<ValueType, RewardModelType> const& dtmc) = default;

    Dtmc(Dtmc<ValueType, RewardModelType>&& dtmc) = default;
    Dtmc& operator=(Dtmc<ValueType, RewardModelType>&& dtmc) = default;

    virtual ~Dtmc() = default;

    virtual void reduceToStateBasedRewards() override;
};

// Instantiated in Dtmc.cpp; prevents hidden consumers from duplicating the vtable/RTTI.
extern template class Dtmc<double>;
extern template class Dtmc<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
extern template class Dtmc<storm::RationalNumber>;
extern template class Dtmc<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
extern template class Dtmc<storm::Interval>;
extern template class Dtmc<storm::RationalInterval>;
extern template class Dtmc<storm::RationalFunction>;

}  // namespace sparse
}  // namespace models
}  // namespace storm
