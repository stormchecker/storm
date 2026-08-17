#include "storm/models/symbolic/Dtmc.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
Dtmc<Type, ValueType>::Dtmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            std::set<storm::expressions::Variable> const& rowVariables,
                            std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
                            std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
                            std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Dtmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, rowExpressionAdapter, columnVariables, rowColumnMetaVariablePairs, labelToExpressionMap, rewardModels) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
Dtmc<Type, ValueType>::Dtmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Dtmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, columnVariables, rowColumnMetaVariablePairs, labelToBddMap, rewardModels) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
void Dtmc<Type, ValueType>::reduceToStateBasedRewards() {
    for (auto& rewardModel : this->getRewardModels()) {
        rewardModel.second.reduceToStateBasedRewards(this->getTransitionMatrix(), this->getRowVariables(), this->getColumnVariables(), true);
    }
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
std::shared_ptr<Dtmc<Type, NewValueType>> Dtmc<Type, ValueType>::toValueType() const {
    typedef typename DeterministicModel<Type, NewValueType>::RewardModelType NewRewardModelType;
    std::unordered_map<std::string, NewRewardModelType> newRewardModels;

    for (auto const& e : this->getRewardModels()) {
        newRewardModels.emplace(e.first, e.second.template toValueType<NewValueType>());
    }

    auto newLabelToBddMap = this->getLabelToBddMap();
    newLabelToBddMap.erase("init");
    newLabelToBddMap.erase("deadlock");

    return std::make_shared<Dtmc<Type, NewValueType>>(this->getManagerAsSharedPointer(), this->getReachableStates(), this->getInitialStates(),
                                                      this->getDeadlockStates(), this->getTransitionMatrix().template toValueType<NewValueType>(),
                                                      this->getRowVariables(), this->getColumnVariables(), this->getRowColumnMetaVariablePairs(),
                                                      newLabelToBddMap, newRewardModels);
}

// Explicitly instantiate the template class.
template class Dtmc<storm::dd::DdType::CUDD, double>;

template class Dtmc<storm::dd::DdType::Sylvan, double>;
template class Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template std::shared_ptr<Dtmc<storm::dd::DdType::Sylvan, double>> Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType() const;
template class Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
