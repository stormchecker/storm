#include "storm/models/symbolic/MarkovAutomaton.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
MarkovAutomaton<Type, ValueType>::MarkovAutomaton(
    std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> markovianMarker, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
    std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : NondeterministicModel<Type, ValueType>(storm::models::ModelType::MarkovAutomaton, manager, reachableStates, initialStates, deadlockStates,
                                             transitionMatrix, rowVariables, rowExpressionAdapter, columnVariables, rowColumnMetaVariablePairs,
                                             nondeterminismVariables, labelToExpressionMap, rewardModels),
      markovianMarker(markovianMarker) {
    // Compute all Markovian info.
    computeMarkovianInfo();
}

template<storm::dd::DdType Type, typename ValueType>
MarkovAutomaton<Type, ValueType>::MarkovAutomaton(
    std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> markovianMarker, storm::dd::Bdd<Type> reachableStates,
    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
    std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& nondeterminismVariables, std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap,
    std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : NondeterministicModel<Type, ValueType>(storm::models::ModelType::MarkovAutomaton, manager, reachableStates, initialStates, deadlockStates,
                                             transitionMatrix, rowVariables, columnVariables, rowColumnMetaVariablePairs, nondeterminismVariables,
                                             labelToBddMap, rewardModels),
      markovianMarker(markovianMarker) {
    // Compute all Markovian info.
    computeMarkovianInfo();
}

template<storm::dd::DdType Type, typename ValueType>
void MarkovAutomaton<Type, ValueType>::computeMarkovianInfo() {
    // Compute the Markovian choices.
    this->markovianChoices = this->getQualitativeTransitionMatrix() && this->markovianMarker;

    // Compute the probabilistic states.
    std::set<storm::expressions::Variable> columnAndNondeterminsmVariables;
    std::set_union(this->getColumnVariables().begin(), this->getColumnVariables().end(), this->getNondeterminismVariables().begin(),
                   this->getNondeterminismVariables().end(), std::inserter(columnAndNondeterminsmVariables, columnAndNondeterminsmVariables.begin()));
    this->probabilisticStates = (this->getQualitativeTransitionMatrix() && !markovianMarker).existsAbstract(columnAndNondeterminsmVariables);

    // Compute the Markovian states.
    this->markovianStates = markovianChoices.existsAbstract(columnAndNondeterminsmVariables);

    // Compute the vector of exit rates.
    this->exitRateVector = (this->getTransitionMatrix() * this->markovianMarker.template toAdd<ValueType>()).sumAbstract(columnAndNondeterminsmVariables);

    // Modify the transition matrix so all choices are probabilistic and the Markovian choices additionally
    // have a rate.
    this->transitionMatrix = this->transitionMatrix / this->markovianChoices.ite(this->exitRateVector, this->getManager().template getAddOne<ValueType>());
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& MarkovAutomaton<Type, ValueType>::getMarkovianMarker() const {
    return this->markovianMarker;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& MarkovAutomaton<Type, ValueType>::getMarkovianStates() const {
    return this->markovianStates;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& MarkovAutomaton<Type, ValueType>::getMarkovianChoices() const {
    return this->markovianChoices;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& MarkovAutomaton<Type, ValueType>::getProbabilisticStates() const {
    return this->markovianStates;
}

template<storm::dd::DdType Type, typename ValueType>
bool MarkovAutomaton<Type, ValueType>::hasHybridStates() const {
    return !(this->probabilisticStates && this->markovianStates).isZero();
}

template<storm::dd::DdType Type, typename ValueType>
bool MarkovAutomaton<Type, ValueType>::isClosed() {
    return !this->hasHybridStates();
}

template<storm::dd::DdType Type, typename ValueType>
MarkovAutomaton<Type, ValueType> MarkovAutomaton<Type, ValueType>::close() {
    // Create the new transition matrix by deleting all Markovian transitions from probabilistic states.
    storm::dd::Add<Type, ValueType> newTransitionMatrix = this->probabilisticStates.ite(
        this->getTransitionMatrix() * (!this->getMarkovianMarker()).template toAdd<ValueType>(), this->getTransitionMatrix() * this->getExitRateVector());

    return MarkovAutomaton<Type, ValueType>(this->getManagerAsSharedPointer(), this->getMarkovianMarker(), this->getReachableStates(), this->getInitialStates(),
                                            this->getDeadlockStates(), newTransitionMatrix, this->getRowVariables(), this->getRowExpressionAdapter(),
                                            this->getColumnVariables(), this->getRowColumnMetaVariablePairs(), this->getNondeterminismVariables(),
                                            this->getLabelToExpressionMap(), this->getRewardModels());
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> const& MarkovAutomaton<Type, ValueType>::getExitRateVector() const {
    return this->exitRateVector;
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
std::shared_ptr<MarkovAutomaton<Type, NewValueType>> MarkovAutomaton<Type, ValueType>::toValueType() const {
    typedef typename NondeterministicModel<Type, NewValueType>::RewardModelType NewRewardModelType;
    std::unordered_map<std::string, NewRewardModelType> newRewardModels;

    for (auto const& e : this->getRewardModels()) {
        newRewardModels.emplace(e.first, e.second.template toValueType<NewValueType>());
    }

    auto newLabelToBddMap = this->getLabelToBddMap();
    newLabelToBddMap.erase("init");
    newLabelToBddMap.erase("deadlock");

    return std::make_shared<MarkovAutomaton<Type, NewValueType>>(
        this->getManagerAsSharedPointer(), this->getMarkovianMarker(), this->getReachableStates(), this->getInitialStates(), this->getDeadlockStates(),
        this->getTransitionMatrix().template toValueType<NewValueType>(), this->getRowVariables(), this->getColumnVariables(),
        this->getRowColumnMetaVariablePairs(), this->getNondeterminismVariables(), newLabelToBddMap, newRewardModels);
}

// Explicitly instantiate the template class.
template class MarkovAutomaton<storm::dd::DdType::CUDD, double>;

template class MarkovAutomaton<storm::dd::DdType::Sylvan, double>;
template class MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template std::shared_ptr<MarkovAutomaton<storm::dd::DdType::Sylvan, double>> MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType()
    const;
template class MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
