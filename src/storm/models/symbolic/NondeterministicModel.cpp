#include "storm/models/symbolic/NondeterministicModel.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
NondeterministicModel<Type, ValueType>::NondeterministicModel(
    storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
    std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Model<Type, ValueType>(modelType, manager, reachableStates, initialStates, deadlockStates, transitionMatrix, rowVariables, rowExpressionAdapter,
                             columnVariables, rowColumnMetaVariablePairs, labelToExpressionMap, rewardModels),
      nondeterminismVariables(nondeterminismVariables) {
    createIllegalMask();
}

template<storm::dd::DdType Type, typename ValueType>
NondeterministicModel<Type, ValueType>::NondeterministicModel(
    storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : Model<Type, ValueType>(modelType, manager, reachableStates, initialStates, deadlockStates, transitionMatrix, rowVariables, columnVariables,
                             rowColumnMetaVariablePairs, labelToBddMap, rewardModels),
      nondeterminismVariables(nondeterminismVariables) {
    createIllegalMask();
}

template<storm::dd::DdType Type, typename ValueType>
uint_fast64_t NondeterministicModel<Type, ValueType>::getNumberOfChoices() const {
    std::set<storm::expressions::Variable> rowAndNondeterminismVariables;
    std::set_union(this->getNondeterminismVariables().begin(), this->getNondeterminismVariables().end(), this->getRowVariables().begin(),
                   this->getRowVariables().end(), std::inserter(rowAndNondeterminismVariables, rowAndNondeterminismVariables.begin()));

    storm::dd::Add<Type, uint_fast64_t> tmp = this->getTransitionMatrix()
                                                  .notZero()
                                                  .existsAbstract(this->getColumnVariables())
                                                  .template toAdd<uint_fast64_t>()
                                                  .sumAbstract(rowAndNondeterminismVariables);
    return tmp.getValue();
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& NondeterministicModel<Type, ValueType>::getNondeterminismVariables() const {
    return nondeterminismVariables;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& NondeterministicModel<Type, ValueType>::getIllegalMask() const {
    return illegalMask;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> NondeterministicModel<Type, ValueType>::getIllegalSuccessorMask() const {
    storm::dd::Bdd<Type> transitionMatrixBdd = this->getTransitionMatrix().notZero();
    return !transitionMatrixBdd && transitionMatrixBdd.existsAbstract(this->getColumnVariables());
}

template<storm::dd::DdType Type, typename ValueType>
void NondeterministicModel<Type, ValueType>::printModelInformationToStream(std::ostream& out) const {
    this->printModelInformationHeaderToStream(out);
    out << "Choices: \t" << this->getNumberOfChoices() << '\n';
    this->printModelInformationFooterToStream(out);
}

template<storm::dd::DdType Type, typename ValueType>
void NondeterministicModel<Type, ValueType>::printDdVariableInformationToStream(std::ostream& out) const {
    uint_fast64_t nondeterminismVariableCount = 0;
    for (auto const& metaVariable : this->getNondeterminismVariables()) {
        nondeterminismVariableCount += this->getManager().getMetaVariable(metaVariable).getNumberOfDdVariables();
    }
    Model<Type, ValueType>::printDdVariableInformationToStream(out);
    out << ", nondeterminism: " << this->getNondeterminismVariables().size() << " meta variables (" << nondeterminismVariableCount << " DD variables)";
}

template<storm::dd::DdType Type, typename ValueType>
void NondeterministicModel<Type, ValueType>::reduceToStateBasedRewards() {
    for (auto& rewardModel : this->getRewardModels()) {
        rewardModel.second.reduceToStateBasedRewards(this->getTransitionMatrix(), this->getRowVariables(), this->getColumnVariables(), false);
    }
}

template<storm::dd::DdType Type, typename ValueType>
void NondeterministicModel<Type, ValueType>::createIllegalMask() {
    // Prepare the mask of illegal nondeterministic choices.
    illegalMask = !(this->getTransitionMatrix().notZero().existsAbstract(this->getColumnVariables())) && this->getReachableStates();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> NondeterministicModel<Type, ValueType>::getQualitativeTransitionMatrix(bool keepNondeterminism) const {
    if (!keepNondeterminism) {
        return this->getTransitionMatrix().notZero().existsAbstract(this->getNondeterminismVariables());
    } else {
        return Model<Type, ValueType>::getQualitativeTransitionMatrix(keepNondeterminism);
    }
}

// Explicitly instantiate the template class.
template class NondeterministicModel<storm::dd::DdType::CUDD, double>;

template class NondeterministicModel<storm::dd::DdType::Sylvan, double>;
template class NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace symbolic
}  // namespace models
}  // namespace storm
