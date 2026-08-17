#pragma once

#include "storm/models/symbolic/NondeterministicModel.h"

namespace storm {
namespace models {
namespace symbolic {

/*!
 * This class represents a discrete-time stochastic two-player game.
 */
template<storm::dd::DdType Type, typename ValueType = double>
class StochasticTwoPlayerGame : public NondeterministicModel<Type, ValueType> {
   public:
    typedef typename NondeterministicModel<Type, ValueType>::RewardModelType RewardModelType;

    StochasticTwoPlayerGame(StochasticTwoPlayerGame<Type, ValueType> const& other) = default;
    StochasticTwoPlayerGame& operator=(StochasticTwoPlayerGame<Type, ValueType> const& other) = default;

    StochasticTwoPlayerGame(StochasticTwoPlayerGame<Type, ValueType>&& other) = default;
    StochasticTwoPlayerGame& operator=(StochasticTwoPlayerGame<Type, ValueType>&& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param rowExpressionAdapter An object that can be used to translate expressions in terms of the row
     * meta variables.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param player1Variables The meta variables used to encode the nondeterministic choices of player 1.
     * @param player2Variables The meta variables used to encode the nondeterministic choices of player 2.
     * @param allNondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToExpressionMap A mapping from label names to their defining expressions.
     * @param rewardModels The reward models associated with the model.
     */
    StochasticTwoPlayerGame(
        std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
        storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
        std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
        std::set<storm::expressions::Variable> const& columnVariables,
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
        std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables,
        std::set<storm::expressions::Variable> const& allNondeterminismVariables,
        std::map<std::string, storm::expressions::Expression> labelToExpressionMap = std::map<std::string, storm::expressions::Expression>(),
        std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param player1Variables The meta variables used to encode the nondeterministic choices of player 1.
     * @param player2Variables The meta variables used to encode the nondeterministic choices of player 2.
     * @param allNondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToBddMap A mapping from label names to their defining BDDs.
     * @param rewardModels The reward models associated with the model.
     */
    StochasticTwoPlayerGame(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables,
                            std::set<storm::expressions::Variable> const& allNondeterminismVariables,
                            std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap = std::map<std::string, storm::dd::Bdd<Type>>(),
                            std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Retrieeves the set of meta variables used to encode the nondeterministic choices of player 1.
     *
     * @return The set of meta variables used to encode the nondeterministic choices of player 1.
     */
    std::set<storm::expressions::Variable> const& getPlayer1Variables() const;

    /*!
     * Retrieeves the set of meta variables used to encode the nondeterministic choices of player 2.
     *
     * @return The set of meta variables used to encode the nondeterministic choices of player 2.
     */
    std::set<storm::expressions::Variable> const& getPlayer2Variables() const;

    /*!
     * Retrieves a BDD characterizing all illegal player 1 choice encodings in the model.
     *
     * @return A BDD characterizing all illegal player 1 choice encodings in the model.
     */
    storm::dd::Bdd<Type> getIllegalPlayer1Mask() const;

    /*!
     * Retrieves a BDD characterizing all illegal player 2 choice encodings in the model.
     *
     * @return A BDD characterizing all illegal player 2 choice encodings in the model.
     */
    storm::dd::Bdd<Type> getIllegalPlayer2Mask() const;

    template<typename NewValueType>
    std::shared_ptr<StochasticTwoPlayerGame<Type, NewValueType>> toValueType() const;

    /*!
     * Retrieves the number of player 2 states in the game.
     */
    uint64_t getNumberOfPlayer2States() const;

   private:
    /*!
     * Prepare all illegal masks.
     */
    void createIllegalMasks();

    // A mask that characterizes all illegal player 1 choices.
    storm::dd::Bdd<Type> illegalPlayer1Mask;

    // A mask that characterizes all illegal player 2 choices.
    storm::dd::Bdd<Type> illegalPlayer2Mask;

    // The meta variables used to encode the nondeterministic choices of player 1.
    std::set<storm::expressions::Variable> player1Variables;

    // The meta variables used to encode the nondeterministic choices of player 2.
    std::set<storm::expressions::Variable> player2Variables;
};

}  // namespace symbolic
}  // namespace models
}  // namespace storm
