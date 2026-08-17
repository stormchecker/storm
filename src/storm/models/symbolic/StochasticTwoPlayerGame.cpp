#include "storm/models/symbolic/StochasticTwoPlayerGame.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
StochasticTwoPlayerGame<Type, ValueType>::StochasticTwoPlayerGame(
    std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
    storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
    std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter, std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : NondeterministicModel<Type, ValueType>(storm::models::ModelType::S2pg, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                             rowVariables, rowExpressionAdapter, columnVariables, rowColumnMetaVariablePairs, nondeterminismVariables,
                                             labelToExpressionMap, rewardModels),
      player1Variables(player1Variables),
      player2Variables(player2Variables) {
    createIllegalMasks();
}

template<storm::dd::DdType Type, typename ValueType>
StochasticTwoPlayerGame<Type, ValueType>::StochasticTwoPlayerGame(
    std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
    storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
    std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : NondeterministicModel<Type, ValueType>(storm::models::ModelType::S2pg, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                             rowVariables, columnVariables, rowColumnMetaVariablePairs, nondeterminismVariables, labelToBddMap, rewardModels),
      player1Variables(player1Variables),
      player2Variables(player2Variables) {
    createIllegalMasks();
}

template<storm::dd::DdType Type, typename ValueType>
void StochasticTwoPlayerGame<Type, ValueType>::createIllegalMasks() {
    // Compute legal player 1 mask.
    this->illegalPlayer1Mask = this->getTransitionMatrix().notZero().existsAbstract(this->getColumnVariables()).existsAbstract(this->getPlayer2Variables());

    // Correct the mask for player 2. This is necessary, because it is not yet restricted to the legal choices of player 1.
    illegalPlayer2Mask = this->getIllegalMask() && this->illegalPlayer1Mask;

    // Then set the illegal mask for player 1 correctly.
    this->illegalPlayer1Mask = !illegalPlayer1Mask && this->getReachableStates();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> StochasticTwoPlayerGame<Type, ValueType>::getIllegalPlayer1Mask() const {
    return illegalPlayer1Mask;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> StochasticTwoPlayerGame<Type, ValueType>::getIllegalPlayer2Mask() const {
    return illegalPlayer2Mask;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& StochasticTwoPlayerGame<Type, ValueType>::getPlayer1Variables() const {
    return player1Variables;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& StochasticTwoPlayerGame<Type, ValueType>::getPlayer2Variables() const {
    return player2Variables;
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
std::shared_ptr<StochasticTwoPlayerGame<Type, NewValueType>> StochasticTwoPlayerGame<Type, ValueType>::toValueType() const {
    typedef typename NondeterministicModel<Type, NewValueType>::RewardModelType NewRewardModelType;
    std::unordered_map<std::string, NewRewardModelType> newRewardModels;

    for (auto const& e : this->getRewardModels()) {
        newRewardModels.emplace(e.first, e.second.template toValueType<NewValueType>());
    }

    auto newLabelToBddMap = this->getLabelToBddMap();
    newLabelToBddMap.erase("init");
    newLabelToBddMap.erase("deadlock");

    return std::make_shared<StochasticTwoPlayerGame<Type, NewValueType>>(
        this->getManagerAsSharedPointer(), this->getReachableStates(), this->getInitialStates(), this->getDeadlockStates(),
        this->getTransitionMatrix().template toValueType<NewValueType>(), this->getRowVariables(), this->getColumnVariables(),
        this->getRowColumnMetaVariablePairs(), this->getPlayer1Variables(), this->getPlayer2Variables(), this->getNondeterminismVariables(), newLabelToBddMap,
        newRewardModels);
}

template<storm::dd::DdType Type, typename ValueType>
uint64_t StochasticTwoPlayerGame<Type, ValueType>::getNumberOfPlayer2States() const {
    return this->getQualitativeTransitionMatrix().existsAbstract(this->getColumnVariables()).getNonZeroCount();
}

// Explicitly instantiate the template class.
template class StochasticTwoPlayerGame<storm::dd::DdType::CUDD, double>;

template class StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, double>;
template class StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template std::shared_ptr<StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, double>>
StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType<double>() const;
template class StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
