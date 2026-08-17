#pragma once

#include "storm/models/sparse/NondeterministicModel.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a (discrete-time) stochastic two-player game.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class StochasticTwoPlayerGame : public NondeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param player1Matrix The matrix representing the choices of player 1.
     * @param player2Matrix The matrix representing the choices of player 2.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    StochasticTwoPlayerGame(storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& player1Matrix,
                            storm::storage::SparseMatrix<ValueType> const& player2Matrix, storm::models::sparse::StateLabeling const& stateLabeling,
                            std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>()),

        /*!
         * Constructs a model by moving the given data.
         *
         * @param player1Matrix The matrix representing the choices of player 1.
         * @param player2Matrix The matrix representing the choices of player 2.
         * @param stateLabeling The labeling of the states.
         * @param rewardModels A mapping of reward model names to reward models.
         */
        StochasticTwoPlayerGame(storm::storage::SparseMatrix<storm::storage::sparse::state_type>&& player1Matrix,
                                storm::storage::SparseMatrix<ValueType>&& player2Matrix, storm::models::sparse::StateLabeling&& stateLabeling,
                                std::unordered_map<std::string, RewardModelType>&& rewardModels = std::unordered_map<std::string, RewardModelType>()),

        /*!
         * Constructs a model from the given data.
         *
         * @param components The components for this model.
         */
        StochasticTwoPlayerGame(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    StochasticTwoPlayerGame(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    StochasticTwoPlayerGame(StochasticTwoPlayerGame const& other) = default;
    StochasticTwoPlayerGame& operator=(StochasticTwoPlayerGame const& other) = default;

    StochasticTwoPlayerGame(StochasticTwoPlayerGame&& other) = default;
    StochasticTwoPlayerGame& operator=(StochasticTwoPlayerGame&& other) = default;

    /*!
     * Retrieves the matrix representing the choices in player 1 states.
     *
     * @return A matrix representing the choices in player 1 states.
     */
    storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& getPlayer1Matrix() const;

    /*!
     * Retrieves the matrix representing the choices in player 2 states and the associated probability
     * distributions.
     *
     * @return A matrix representing the choices in player 2 states.
     */
    storm::storage::SparseMatrix<ValueType> const& getPlayer2Matrix() const;

    /*!
     * Retrieves whether the game has labels attached to player 2 states.
     *
     * @return True if the game has player 2 labels.
     */
    bool hasPlayer2ChoiceLabeling() const;

    /*!
     * Retrieves the labels attached to the choices of player 2 states.
     *
     * @return A vector containing the labels of each player 2 choice.
     */
    storm::models::sparse::ChoiceLabeling const& getPlayer2ChoiceLabeling() const;

   private:
    // A matrix that stores the player 1 choices. This matrix contains a row group for each player 1 node. Every
    // row group contains a row for each choice in that player 1 node. Each such row contains exactly one
    // (non-zero) entry at a column that indicates the player 2 node this choice leads to (which is essentially
    // the index of a row group in the matrix for player 2).
    storm::storage::SparseMatrix<storm::storage::sparse::state_type> player1Matrix;

    // The matrix for player 2 are stored in the superclass.
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
