#pragma once

#include "storm/models/sparse/DeterministicModel.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a continuous-time Markov chain.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class Ctmc : public DeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param rateMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Ctmc(storm::storage::SparseMatrix<ValueType> const& rateMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model by moving the given data.
     *
     * @param rateMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Ctmc(storm::storage::SparseMatrix<ValueType>&& rateMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
         std::unordered_map<std::string, RewardModelType>&& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    Ctmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    Ctmc(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    Ctmc(Ctmc<ValueType, RewardModelType> const& ctmc) = default;
    Ctmc& operator=(Ctmc<ValueType, RewardModelType> const& ctmc) = default;
    Ctmc(Ctmc<ValueType, RewardModelType>&& ctmc) = default;
    Ctmc& operator=(Ctmc<ValueType, RewardModelType>&& ctmc) = default;

    /*!
     * Retrieves the vector of exit rates of the model.
     *
     * @return The exit rate vector.
     */
    std::vector<ValueType> const& getExitRateVector() const;

    /*!
     * Retrieves the vector of exit rates of the model.
     *
     * @return The exit rate vector.
     */
    std::vector<ValueType>& getExitRateVector();

    virtual void reduceToStateBasedRewards() override;

    /*!
     * @return the probabilistic transition matrix P
     * @note getTransitionMatrix() retrieves the exit rate matrix R, where R(s,s') = r(s) * P(s,s')
     */
    storm::storage::SparseMatrix<ValueType> computeProbabilityMatrix() const;

   private:
    /*!
     * Computes the exit rate vector based on the given rate matrix.
     *
     * @param rateMatrix The rate matrix.
     * @return The exit rate vector.
     */
    static std::vector<ValueType> createExitRateVector(storm::storage::SparseMatrix<ValueType> const& rateMatrix);

    // A vector containing the exit rates of all states.
    std::vector<ValueType> exitRates;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
