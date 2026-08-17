#include "storm/models/symbolic/Ctmc.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
Ctmc<Type, ValueType>::Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            std::set<storm::expressions::Variable> const& rowVariables,
                            std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
                            std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
                            std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Ctmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, rowExpressionAdapter, columnVariables, rowColumnMetaVariablePairs, labelToExpressionMap, rewardModels) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
Ctmc<Type, ValueType>::Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            boost::optional<storm::dd::Add<Type, ValueType>> exitRateVector, std::set<storm::expressions::Variable> const& rowVariables,
                            std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
                            std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
                            std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Ctmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, rowExpressionAdapter, columnVariables, rowColumnMetaVariablePairs, labelToExpressionMap, rewardModels),
      exitRates(exitRateVector) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
Ctmc<Type, ValueType>::Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Ctmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, columnVariables, rowColumnMetaVariablePairs, labelToBddMap, rewardModels) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
Ctmc<Type, ValueType>::Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
                            storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                            boost::optional<storm::dd::Add<Type, ValueType>> exitRateVector, std::set<storm::expressions::Variable> const& rowVariables,
                            std::set<storm::expressions::Variable> const& columnVariables,
                            std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                            std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : DeterministicModel<Type, ValueType>(storm::models::ModelType::Ctmc, manager, reachableStates, initialStates, deadlockStates, transitionMatrix,
                                          rowVariables, columnVariables, rowColumnMetaVariablePairs, labelToBddMap, rewardModels),
      exitRates(exitRateVector) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> const& Ctmc<Type, ValueType>::getExitRateVector() const {
    if (!exitRates) {
        exitRates = this->getTransitionMatrix().sumAbstract(this->getColumnVariables());
    }
    return exitRates.get();
}

template<storm::dd::DdType Type, typename ValueType>
void Ctmc<Type, ValueType>::reduceToStateBasedRewards() {
    for (auto& rewardModel : this->getRewardModels()) {
        if (rewardModel.second.hasStateActionRewards()) {
            rewardModel.second.getStateActionRewardVector() *= getExitRateVector();
        }
        rewardModel.second.reduceToStateBasedRewards(this->getTransitionMatrix(), this->getRowVariables(), this->getColumnVariables(), true);
    }
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> Ctmc<Type, ValueType>::computeProbabilityMatrix() const {
    return this->getTransitionMatrix() / this->getExitRateVector();
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
std::shared_ptr<Ctmc<Type, NewValueType>> Ctmc<Type, ValueType>::toValueType() const {
    typedef typename DeterministicModel<Type, NewValueType>::RewardModelType NewRewardModelType;
    std::unordered_map<std::string, NewRewardModelType> newRewardModels;

    for (auto const& e : this->getRewardModels()) {
        newRewardModels.emplace(e.first, e.second.template toValueType<NewValueType>());
    }

    auto newLabelToBddMap = this->getLabelToBddMap();
    newLabelToBddMap.erase("init");
    newLabelToBddMap.erase("deadlock");

    return std::make_shared<Ctmc<Type, NewValueType>>(this->getManagerAsSharedPointer(), this->getReachableStates(), this->getInitialStates(),
                                                      this->getDeadlockStates(), this->getTransitionMatrix().template toValueType<NewValueType>(),
                                                      this->getExitRateVector().template toValueType<NewValueType>(), this->getRowVariables(),
                                                      this->getColumnVariables(), this->getRowColumnMetaVariablePairs(), newLabelToBddMap, newRewardModels);
}

// Explicitly instantiate the template class.
template class Ctmc<storm::dd::DdType::CUDD, double>;

template class Ctmc<storm::dd::DdType::Sylvan, double>;
template class Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template std::shared_ptr<Ctmc<storm::dd::DdType::Sylvan, double>> Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType() const;
template class Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
