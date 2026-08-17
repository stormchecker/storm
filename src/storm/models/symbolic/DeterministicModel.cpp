#include "storm/models/symbolic/DeterministicModel.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
DeterministicModel<Type, ValueType>::DeterministicModel(
    storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
    std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::map<std::string, storm::expressions::Expression> labelToExpressionMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Model<Type, ValueType>(modelType, manager, reachableStates, initialStates, deadlockStates, transitionMatrix, rowVariables, rowExpressionAdapter,
                             columnVariables, rowColumnMetaVariablePairs, labelToExpressionMap, rewardModels) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
DeterministicModel<Type, ValueType>::DeterministicModel(
    storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Model<Type, ValueType>(modelType, manager, reachableStates, initialStates, deadlockStates, transitionMatrix, rowVariables, columnVariables,
                             rowColumnMetaVariablePairs, labelToBddMap, rewardModels) {
    // Intentionally left empty.
}

// Explicitly instantiate the template class.
template class DeterministicModel<storm::dd::DdType::CUDD>;

template class DeterministicModel<storm::dd::DdType::Sylvan>;
template class DeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class DeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
