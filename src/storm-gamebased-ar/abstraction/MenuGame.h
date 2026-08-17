#pragma once

#include <map>

#include "storm/models/symbolic/StochasticTwoPlayerGame.h"

namespace storm::gbar {
namespace abstraction {

/*!
 * This class represents a discrete-time stochastic two-player game.
 */
template<storm::dd::DdType Type, typename ValueType>
class MenuGame : public storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType> {
   public:
    typedef typename storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType>::RewardModelType RewardModelType;

    MenuGame(MenuGame<Type, ValueType> const& other) = default;
    MenuGame& operator=(MenuGame<Type, ValueType> const& other) = default;
    MenuGame(MenuGame<Type, ValueType>&& other) = default;
    MenuGame& operator=(MenuGame<Type, ValueType>&& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates The reachable states of the model.
     * @param initialStates The initial states of the model.
     * @param deadlockStates The deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param bottomStates The bottom states of the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param player1Variables The meta variables used to encode the nondeterministic choices of player 1.
     * @param player2Variables The meta variables used to encode the nondeterministic choices of player 2.
     * @param allNondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param probabilisticBranchingVariables The variables used to encode probabilistic branching.
     * @param expressionToBddMap A mapping from expressions (used) in the abstraction to the BDDs encoding
     * them.
     */
    MenuGame(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
             storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, storm::dd::Bdd<Type> bottomStates,
             std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
             std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
             std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables,
             std::set<storm::expressions::Variable> const& allNondeterminismVariables,
             std::set<storm::expressions::Variable> const& probabilisticBranchingVariables,
             std::map<storm::expressions::Expression, storm::dd::Bdd<Type>> const& expressionToBddMap);

    virtual storm::dd::Bdd<Type> getStates(std::string const& label) const override;

    /*!
     * Returns the set of states satisfying the given expression (that must be of boolean type). Note that
     * for menu games, the given expression must be a predicate that was used to build the abstract game.
     *
     * @param expression The expression that needs to hold in the states.
     * @return The set of states satisfying the given expression.
     */
    virtual storm::dd::Bdd<Type> getStates(storm::expressions::Expression const& expression) const override;

    /*!
     * Returns the set of states satisfying the given expression (that must be of boolean type). Note that
     * for menu games, the given expression must be a predicate that was used to build the abstract game.
     *
     * @param expression The expression that needs to hold in the states.
     * @param negated If set to true, the result is the set of states not satisfying the expression.
     * @return The set of states labeled satisfying the given expression.
     */
    storm::dd::Bdd<Type> getStates(storm::expressions::Expression const& expression, bool negated) const;

    /*!
     * Retrieves the bottom states of the model.
     *
     * @return The bottom states of the model.
     */
    storm::dd::Bdd<Type> getBottomStates() const;

    /*!
     * Retrieves the transition matrix extended by variables that encode additional information for the
     * probabilistic branching.
     *
     * @reutrn Th extended transition matrix.
     */
    storm::dd::Add<Type, ValueType> const& getExtendedTransitionMatrix() const;

    /*!
     * Retrieves the variables used to encode additional information for the probabilistic branching in the
     * extended transition matrix.
     *
     * @return The probabilistic branching variables.
     */
    std::set<storm::expressions::Variable> const& getProbabilisticBranchingVariables() const;

    virtual bool hasLabel(std::string const& label) const override;

   private:
    // The transition relation extended byt the probabilistic branching variables.
    storm::dd::Add<Type, ValueType> extendedTransitionMatrix;

    // The meta variables used to probabilistic branching.
    std::set<storm::expressions::Variable> probabilisticBranchingVariables;

    // A mapping from expressions that were used in the abstraction process to the the BDDs representing them.
    std::map<storm::expressions::Expression, storm::dd::Bdd<Type>> expressionToBddMap;

    // The bottom states of the model.
    storm::dd::Bdd<Type> bottomStates;
};

}  // namespace abstraction
}  // namespace storm::gbar
