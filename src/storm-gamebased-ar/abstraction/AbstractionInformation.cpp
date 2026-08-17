#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"

#include "storm/environment/Environment.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/dd/DdManager.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
AbstractionInformation<DdType>::AbstractionInformation(storm::Environment const& env, storm::expressions::ExpressionManager& expressionManager,
                                                       std::set<storm::expressions::Variable> const& abstractedVariables,
                                                       std::unique_ptr<storm::solver::SmtSolver>&& smtSolver, AbstractionInformationOptions const& options)
    : expressionManager(expressionManager),
      equivalenceChecker(std::move(smtSolver)),
      abstractedVariables(abstractedVariables),
      constraints(options.constraints),
      ddManager(std::make_shared<storm::dd::DdManager<DdType>>(env)),
      allPredicateIdentities(ddManager->getBddOne()),
      allLocationIdentities(ddManager->getBddOne()),
      expressionToBddMap() {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType>
void AbstractionInformation<DdType>::addExpressionVariable(storm::expressions::Variable const& variable) {
    abstractedVariables.insert(variable);
}

template<storm::dd::DdType DdType>
void AbstractionInformation<DdType>::addExpressionVariable(storm::expressions::Variable const& variable, storm::expressions::Expression const& constraint) {
    addExpressionVariable(variable);
    addConstraint(constraint);
}

template<storm::dd::DdType DdType>
void AbstractionInformation<DdType>::addConstraint(storm::expressions::Expression const& constraint) {
    constraints.push_back(constraint);
}

template<storm::dd::DdType DdType>
uint_fast64_t AbstractionInformation<DdType>::getOrAddPredicate(storm::expressions::Expression const& predicate) {
    // Check if we already have an equivalent predicate.
    for (uint64_t index = 0; index < predicates.size(); ++index) {
        auto const& oldPredicate = predicates[index];
        if (equivalenceChecker.areEquivalent(oldPredicate, predicate)) {
            expressionToBddMap[predicate] = expressionToBddMap.at(oldPredicate);
            return index;
        }
    }

    std::size_t predicateIndex = predicates.size();
    predicateToIndexMap[predicate] = predicateIndex;

    // Add the new predicate to the list of known predicates.
    predicates.push_back(predicate);

    // Add DD variables for the new predicate.
    std::stringstream stream;
    stream << predicate;
    std::pair<storm::expressions::Variable, storm::expressions::Variable> newMetaVariable = ddManager->addMetaVariable(stream.str());

    predicateDdVariables.push_back(newMetaVariable);
    extendedPredicateDdVariables.push_back(newMetaVariable);
    predicateBdds.emplace_back(ddManager->getEncoding(newMetaVariable.first, 1), ddManager->getEncoding(newMetaVariable.second, 1));
    predicateIdentities.push_back(ddManager->getEncoding(newMetaVariable.first, 1).iff(ddManager->getEncoding(newMetaVariable.second, 1)));
    allPredicateIdentities &= predicateIdentities.back();
    sourceVariables.insert(newMetaVariable.first);
    successorVariables.insert(newMetaVariable.second);
    sourcePredicateVariables.insert(newMetaVariable.first);
    successorPredicateVariables.insert(newMetaVariable.second);
    orderedSourcePredicateVariables.push_back(newMetaVariable.first);
    orderedSuccessorPredicateVariables.push_back(newMetaVariable.second);
    ddVariableIndexToPredicateIndexMap[predicateIdentities.back().getIndex()] = predicateIndex;
    expressionToBddMap[predicate] = predicateBdds[predicateIndex].first && !bottomStateBdds.first;

    return predicateIndex;
}

template<storm::dd::DdType DdType>
std::vector<uint_fast64_t> AbstractionInformation<DdType>::addPredicates(std::vector<storm::expressions::Expression> const& predicates) {
    std::vector<uint_fast64_t> predicateIndices;
    for (auto const& predicate : predicates) {
        predicateIndices.push_back(this->getOrAddPredicate(predicate));
    }
    return predicateIndices;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Expression> const& AbstractionInformation<DdType>::getConstraints() const {
    return constraints;
}

template<storm::dd::DdType DdType>
storm::expressions::ExpressionManager& AbstractionInformation<DdType>::getExpressionManager() {
    return expressionManager.get();
}

template<storm::dd::DdType DdType>
storm::expressions::ExpressionManager const& AbstractionInformation<DdType>::getExpressionManager() const {
    return expressionManager.get();
}

template<storm::dd::DdType DdType>
storm::dd::DdManager<DdType>& AbstractionInformation<DdType>::getDdManager() {
    return *ddManager;
}

template<storm::dd::DdType DdType>
storm::dd::DdManager<DdType> const& AbstractionInformation<DdType>::getDdManager() const {
    return *ddManager;
}

template<storm::dd::DdType DdType>
std::shared_ptr<storm::dd::DdManager<DdType>> AbstractionInformation<DdType>::getDdManagerAsSharedPointer() {
    return ddManager;
}

template<storm::dd::DdType DdType>
std::shared_ptr<storm::dd::DdManager<DdType> const> AbstractionInformation<DdType>::getDdManagerAsSharedPointer() const {
    return ddManager;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Expression> const& AbstractionInformation<DdType>::getPredicates() const {
    return predicates;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Expression> AbstractionInformation<DdType>::getPredicates(storm::storage::BitVector const& predicateValuation) const {
    STORM_LOG_ASSERT(predicateValuation.size() == this->getNumberOfPredicates(), "Size of predicate valuation does not match number of predicates.");

    std::vector<storm::expressions::Expression> result;
    for (uint64_t index = 0; index < this->getNumberOfPredicates(); ++index) {
        if (predicateValuation[index]) {
            result.push_back(this->getPredicateByIndex(index));
        } else {
            result.push_back(!this->getPredicateByIndex(index));
        }
    }

    return result;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Expression> AbstractionInformation<DdType>::getPredicatesExcludingBottom(
    storm::storage::BitVector const& predicateValuation) const {
    uint64_t offset = 1 + this->getNumberOfDdSourceLocationVariables();
    STORM_LOG_ASSERT(predicateValuation.size() == this->getNumberOfPredicates() + offset, "Size of predicate valuation does not match number of predicates.");

    std::vector<storm::expressions::Expression> result;
    for (uint64_t index = 0; index < this->getNumberOfPredicates(); ++index) {
        if (predicateValuation[index + offset]) {
            result.push_back(this->getPredicateByIndex(index));
        } else {
            result.push_back(!this->getPredicateByIndex(index));
        }
    }

    return result;
}

template<storm::dd::DdType DdType>
storm::expressions::Expression const& AbstractionInformation<DdType>::getPredicateByIndex(uint_fast64_t index) const {
    return predicates[index];
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::getPredicateSourceVariable(storm::expressions::Expression const& predicate) const {
    auto indexIt = predicateToIndexMap.find(predicate);
    STORM_LOG_THROW(indexIt != predicateToIndexMap.end(), storm::exceptions::InvalidOperationException, "Cannot retrieve BDD for unknown predicate.");
    return predicateBdds[indexIt->second].first;
}

template<storm::dd::DdType DdType>
bool AbstractionInformation<DdType>::hasPredicate(storm::expressions::Expression const& predicate) const {
    return predicateToIndexMap.find(predicate) != predicateToIndexMap.end();
}

template<storm::dd::DdType DdType>
std::size_t AbstractionInformation<DdType>::getNumberOfPredicates() const {
    return predicates.size();
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getAbstractedVariables() const {
    return abstractedVariables;
}

template<storm::dd::DdType DdType>
void AbstractionInformation<DdType>::createEncodingVariables(uint64_t player1VariableCount, uint64_t player2VariableCount, uint64_t auxVariableCount) {
    STORM_LOG_THROW(player1Variables.empty() && player2Variables.empty() && auxVariables.empty(), storm::exceptions::InvalidOperationException,
                    "Variables have already been created.");

    for (uint64_t index = 0; index < player1VariableCount; ++index) {
        storm::expressions::Variable newVariable = ddManager->addMetaVariable("pl1." + std::to_string(index)).first;
        player1Variables.push_back(newVariable);
        player1VariableBdds.push_back(ddManager->getEncoding(newVariable, 1));
    }
    STORM_LOG_DEBUG("Created " << player1VariableCount << " player 1 variables.");

    for (uint64_t index = 0; index < player2VariableCount; ++index) {
        storm::expressions::Variable newVariable = ddManager->addMetaVariable("pl2." + std::to_string(index)).first;
        player2Variables.push_back(newVariable);
        player2VariableBdds.push_back(ddManager->getEncoding(newVariable, 1));
    }
    STORM_LOG_DEBUG("Created " << player2VariableCount << " player 2 variables.");

    for (uint64_t index = 0; index < auxVariableCount; ++index) {
        storm::expressions::Variable newVariable = ddManager->addMetaVariable("aux_" + std::to_string(index)).first;
        auxVariables.push_back(newVariable);
        auxVariableBdds.push_back(ddManager->getEncoding(newVariable, 1));
    }
    STORM_LOG_DEBUG("Created " << auxVariableCount << " auxiliary variables.");

    bottomStateVariables = ddManager->addMetaVariable("bot");
    bottomStateBdds = std::make_pair(ddManager->getEncoding(bottomStateVariables.first, 1), ddManager->getEncoding(bottomStateVariables.second, 1));
    extendedPredicateDdVariables.push_back(bottomStateVariables);
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::encodePlayer1Choice(uint_fast64_t index, uint_fast64_t end) const {
    return encodeChoice(index, 0, end, player1VariableBdds);
}

template<storm::dd::DdType DdType>
uint_fast64_t AbstractionInformation<DdType>::decodePlayer1Choice(storm::expressions::Valuation const& valuation, uint_fast64_t end) const {
    return decodeChoice(valuation, 0, end, player1Variables);
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::encodePlayer2Choice(uint_fast64_t index, uint_fast64_t start, uint_fast64_t end) const {
    return encodeChoice(index, start, end, player2VariableBdds);
}

template<storm::dd::DdType DdType>
uint_fast64_t AbstractionInformation<DdType>::decodePlayer2Choice(storm::expressions::Valuation const& valuation, uint_fast64_t end) const {
    return decodeChoice(valuation, 0, end, player2Variables);
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::encodeAux(uint_fast64_t index, uint_fast64_t start, uint_fast64_t end) const {
    return encodeChoice(index, start, end, auxVariableBdds);
}

template<storm::dd::DdType DdType>
uint_fast64_t AbstractionInformation<DdType>::decodeAux(storm::expressions::Valuation const& valuation, uint_fast64_t start, uint_fast64_t end) const {
    return decodeChoice(valuation, start, end, auxVariables);
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Variable> const& AbstractionInformation<DdType>::getPlayer1Variables() const {
    return player1Variables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> AbstractionInformation<DdType>::getPlayer1VariableSet(uint_fast64_t count) const {
    return std::set<storm::expressions::Variable>(player1Variables.begin(), player1Variables.begin() + count);
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Variable> const& AbstractionInformation<DdType>::getPlayer2Variables() const {
    return player2Variables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> AbstractionInformation<DdType>::getPlayer2VariableSet(uint_fast64_t count) const {
    return std::set<storm::expressions::Variable>(player2Variables.begin(), player2Variables.begin() + count);
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Variable> const& AbstractionInformation<DdType>::getAuxVariables() const {
    return auxVariables;
}

template<storm::dd::DdType DdType>
storm::expressions::Variable const& AbstractionInformation<DdType>::getAuxVariable(uint_fast64_t index) const {
    return auxVariables[index];
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> AbstractionInformation<DdType>::getAuxVariableSet(uint_fast64_t start, uint_fast64_t end) const {
    return std::set<storm::expressions::Variable>(auxVariables.begin() + start, auxVariables.begin() + end);
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSourceVariables() const {
    return sourceVariables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSuccessorVariables() const {
    return successorVariables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSourcePredicateVariables() const {
    return sourcePredicateVariables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSuccessorPredicateVariables() const {
    return successorPredicateVariables;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Variable> const& AbstractionInformation<DdType>::getOrderedSourcePredicateVariables() const {
    return orderedSourcePredicateVariables;
}

template<storm::dd::DdType DdType>
std::vector<storm::expressions::Variable> const& AbstractionInformation<DdType>::getOrderedSuccessorPredicateVariables() const {
    return orderedSuccessorPredicateVariables;
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& AbstractionInformation<DdType>::getAllPredicateIdentities() const {
    return allPredicateIdentities;
}
template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& AbstractionInformation<DdType>::getAllLocationIdentities() const {
    return allLocationIdentities;
}

template<storm::dd::DdType DdType>
std::size_t AbstractionInformation<DdType>::getPlayer1VariableCount() const {
    return player1Variables.size();
}

template<storm::dd::DdType DdType>
std::size_t AbstractionInformation<DdType>::getPlayer2VariableCount() const {
    return player2Variables.size();
}

template<storm::dd::DdType DdType>
std::size_t AbstractionInformation<DdType>::getAuxVariableCount() const {
    return auxVariables.size();
}

template<storm::dd::DdType DdType>
std::map<storm::expressions::Expression, storm::dd::Bdd<DdType>> const& AbstractionInformation<DdType>::getPredicateToBddMap() const {
    return expressionToBddMap;
}

template<storm::dd::DdType DdType>
std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& AbstractionInformation<DdType>::getSourceSuccessorVariablePairs()
    const {
    return predicateDdVariables;
}

template<storm::dd::DdType DdType>
std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const&
AbstractionInformation<DdType>::getExtendedSourceSuccessorVariablePairs() const {
    return extendedPredicateDdVariables;
}

template<storm::dd::DdType DdType>
storm::expressions::Variable const& AbstractionInformation<DdType>::getBottomStateVariable(bool source) const {
    if (source) {
        return bottomStateVariables.first;
    } else {
        return bottomStateVariables.second;
    }
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::getBottomStateBdd(bool source, bool negated) const {
    if (source) {
        if (negated) {
            return !bottomStateBdds.first;
        } else {
            return bottomStateBdds.first;
        }
    } else {
        if (negated) {
            return !bottomStateBdds.second;
        } else {
            return bottomStateBdds.second;
        }
    }
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& AbstractionInformation<DdType>::encodePredicateAsSource(uint_fast64_t predicateIndex) const {
    return predicateBdds[predicateIndex].first;
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& AbstractionInformation<DdType>::encodePredicateAsSuccessor(uint_fast64_t predicateIndex) const {
    return predicateBdds[predicateIndex].second;
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& AbstractionInformation<DdType>::getPredicateIdentity(uint_fast64_t predicateIndex) const {
    return predicateIdentities[predicateIndex];
}

template<storm::dd::DdType DdType>
storm::expressions::Expression const& AbstractionInformation<DdType>::getPredicateForDdVariableIndex(uint_fast64_t ddVariableIndex) const {
    auto indexIt = ddVariableIndexToPredicateIndexMap.find(ddVariableIndex);
    STORM_LOG_THROW(indexIt != ddVariableIndexToPredicateIndexMap.end(), storm::exceptions::InvalidOperationException, "Unknown DD variable index.");
    return predicates[indexIt->second];
}

template<storm::dd::DdType DdType>
std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> AbstractionInformation<DdType>::declareNewVariables(
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> const& oldPredicates, std::set<uint_fast64_t> const& newPredicates) const {
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> result;

    auto oldIt = oldPredicates.begin();
    auto oldIte = oldPredicates.end();
    auto newIt = newPredicates.begin();
    auto newIte = newPredicates.end();

    for (; newIt != newIte; ++newIt) {
        if (oldIt == oldIte || oldIt->second != *newIt) {
            result.push_back(std::make_pair(expressionManager.get().declareFreshBooleanVariable(), *newIt));
        } else {
            ++oldIt;
        }
    }

    return result;
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::encodeChoice(uint_fast64_t index, uint_fast64_t start, uint_fast64_t end,
                                                                    std::vector<storm::dd::Bdd<DdType>> const& variables) const {
    storm::dd::Bdd<DdType> result = ddManager->getBddOne();
    for (uint_fast64_t bitIndex = end; bitIndex > start; --bitIndex) {
        if ((index & 1) != 0) {
            result &= variables[bitIndex - 1];
        } else {
            result &= !variables[bitIndex - 1];
        }
        index >>= 1;
    }
    STORM_LOG_ASSERT(!result.isZero(), "BDD encoding must not be zero.");
    return result;
}

template<storm::dd::DdType DdType>
uint_fast64_t AbstractionInformation<DdType>::decodeChoice(storm::expressions::Valuation const& valuation, uint_fast64_t start, uint_fast64_t end,
                                                           std::vector<storm::expressions::Variable> const& variables) const {
    uint_fast64_t result = 0;
    for (uint_fast64_t variableIndex = start; variableIndex < end; ++variableIndex) {
        result <<= 1;
        if (valuation.getBooleanValue(variables[variableIndex])) {
            result |= 1;
        }
    }
    return result;
}

template<storm::dd::DdType DdType>
storm::storage::BitVector AbstractionInformation<DdType>::decodeState(storm::dd::Bdd<DdType> const& state) const {
    STORM_LOG_ASSERT(state.getNonZeroCount() == 1, "Wrong number of non-zero entries.");

    storm::storage::BitVector statePredicates(this->getNumberOfPredicates());

    storm::dd::Add<DdType, double> add = state.template toAdd<double>();
    auto it = add.begin();
    auto stateValuePair = *it;
    for (uint_fast64_t index = 0; index < this->getOrderedSourcePredicateVariables().size(); ++index) {
        auto const& successorVariable = this->getOrderedSourcePredicateVariables()[index];
        if (stateValuePair.first.getBooleanValue(successorVariable)) {
            statePredicates.set(index);
        }
    }

    return statePredicates;
}

template<storm::dd::DdType DdType>
template<typename ValueType>
std::map<uint_fast64_t, std::pair<storm::storage::BitVector, ValueType>> AbstractionInformation<DdType>::decodeChoiceToUpdateSuccessorMapping(
    storm::dd::Bdd<DdType> const& choice) const {
    std::map<uint_fast64_t, std::pair<storm::storage::BitVector, ValueType>> result;

    storm::dd::Add<DdType, ValueType> lowerChoiceAsAdd = choice.template toAdd<ValueType>();
    for (auto const& successorValuePair : lowerChoiceAsAdd) {
        uint_fast64_t updateIndex = this->decodeAux(successorValuePair.first, 0, this->getAuxVariableCount());

        storm::storage::BitVector successor(this->getNumberOfPredicates());
        for (uint_fast64_t index = 0; index < this->getOrderedSuccessorPredicateVariables().size(); ++index) {
            auto const& successorVariable = this->getOrderedSuccessorPredicateVariables()[index];
            if (successorValuePair.first.getBooleanValue(successorVariable)) {
                successor.set(index);
            }
        }

        result[updateIndex] = std::make_pair(successor, successorValuePair.second);
    }
    return result;
}

template<storm::dd::DdType DdType>
template<typename ValueType>
std::vector<std::map<uint_fast64_t, std::pair<storm::storage::BitVector, ValueType>>> AbstractionInformation<DdType>::decodeChoicesToUpdateSuccessorMapping(
    std::set<storm::expressions::Variable> const& player2Variables, storm::dd::Bdd<DdType> const& choices) const {
    std::vector<storm::dd::Bdd<DdType>> splitChoices = choices.split(player2Variables);

    std::vector<std::map<uint_fast64_t, std::pair<storm::storage::BitVector, ValueType>>> result;
    for (auto const& choice : splitChoices) {
        result.emplace_back(this->template decodeChoiceToUpdateSuccessorMapping<ValueType>(choice));
    }

    return result;
}

template<storm::dd::DdType DdType>
std::tuple<storm::storage::BitVector, uint64_t, uint64_t> AbstractionInformation<DdType>::decodeStatePlayer1ChoiceAndUpdate(
    storm::dd::Bdd<DdType> const& stateChoiceAndUpdate) const {
    STORM_LOG_ASSERT(stateChoiceAndUpdate.getNonZeroCount() == 1, "Wrong number of non-zero entries.");

    storm::storage::BitVector statePredicates(this->getNumberOfPredicates());

    storm::dd::Add<DdType, double> add = stateChoiceAndUpdate.template toAdd<double>();
    auto it = add.begin();
    auto stateValuePair = *it;
    uint64_t choiceIndex = this->decodePlayer1Choice(stateValuePair.first, this->getPlayer1VariableCount());
    uint64_t updateIndex = this->decodeAux(stateValuePair.first, 0, this->getAuxVariableCount());
    for (uint_fast64_t index = 0; index < this->getOrderedSourcePredicateVariables().size(); ++index) {
        auto const& successorVariable = this->getOrderedSourcePredicateVariables()[index];

        if (stateValuePair.first.getBooleanValue(successorVariable)) {
            statePredicates.set(index);
        }
    }

    return std::make_tuple(statePredicates, choiceIndex, updateIndex);
}

template<storm::dd::DdType DdType>
std::pair<std::pair<storm::expressions::Variable, storm::expressions::Variable>, uint64_t> AbstractionInformation<DdType>::addLocationVariables(
    storm::expressions::Variable const& locationExpressionVariable, uint64_t highestLocationIndex) {
    auto newMetaVariable = ddManager->addMetaVariable("loc_" + std::to_string(locationVariablePairs.size()), 0, highestLocationIndex);

    locationExpressionVariables.insert(locationExpressionVariable);
    locationExpressionToDdVariableMap.emplace(locationExpressionVariable, newMetaVariable);
    locationVariablePairs.emplace_back(newMetaVariable);
    allSourceLocationVariables.insert(newMetaVariable.first);
    sourceVariables.insert(newMetaVariable.first);
    allSuccessorLocationVariables.insert(newMetaVariable.second);
    successorVariables.insert(newMetaVariable.second);
    extendedPredicateDdVariables.emplace_back(newMetaVariable);
    allLocationIdentities &= ddManager->getIdentity(newMetaVariable.first, newMetaVariable.second);
    return std::make_pair(locationVariablePairs.back(), locationVariablePairs.size() - 1);
}

template<storm::dd::DdType DdType>
storm::expressions::Variable AbstractionInformation<DdType>::getLocationVariable(uint64_t locationVariableIndex, bool source) const {
    if (source) {
        return locationVariablePairs[locationVariableIndex].first;
    } else {
        return locationVariablePairs[locationVariableIndex].second;
    }
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSourceLocationVariables() const {
    return allSourceLocationVariables;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getSuccessorLocationVariables() const {
    return allSuccessorLocationVariables;
}

template<storm::dd::DdType DdType>
storm::expressions::Variable const& AbstractionInformation<DdType>::getDdLocationMetaVariable(storm::expressions::Variable const& locationExpressionVariable,
                                                                                              bool source) {
    auto const& metaVariablePair = locationExpressionToDdVariableMap.at(locationExpressionVariable);
    if (source) {
        return metaVariablePair.first;
    } else {
        return metaVariablePair.second;
    }
}

template<storm::dd::DdType DdType>
uint64_t AbstractionInformation<DdType>::getNumberOfDdSourceLocationVariables() const {
    uint64_t result = 0;
    for (auto const& locationVariableToMetaVariablePair : locationExpressionToDdVariableMap) {
        result += ddManager->getMetaVariable(locationVariableToMetaVariablePair.second.first).getNumberOfDdVariables();
    }
    return result;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& AbstractionInformation<DdType>::getLocationExpressionVariables() const {
    return locationExpressionVariables;
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> AbstractionInformation<DdType>::encodeLocation(storm::expressions::Variable const& locationVariable, uint64_t locationIndex) const {
    return this->getDdManager().getEncoding(locationVariable, locationIndex);
}

template class AbstractionInformation<storm::dd::DdType::CUDD>;
template class AbstractionInformation<storm::dd::DdType::Sylvan>;

template std::map<uint_fast64_t, std::pair<storm::storage::BitVector, double>>
AbstractionInformation<storm::dd::DdType::CUDD>::decodeChoiceToUpdateSuccessorMapping(storm::dd::Bdd<storm::dd::DdType::CUDD> const& choice) const;
template std::map<uint_fast64_t, std::pair<storm::storage::BitVector, double>>
AbstractionInformation<storm::dd::DdType::Sylvan>::decodeChoiceToUpdateSuccessorMapping(storm::dd::Bdd<storm::dd::DdType::Sylvan> const& choice) const;
template std::map<uint_fast64_t, std::pair<storm::storage::BitVector, storm::RationalNumber>>
AbstractionInformation<storm::dd::DdType::Sylvan>::decodeChoiceToUpdateSuccessorMapping(storm::dd::Bdd<storm::dd::DdType::Sylvan> const& choice) const;

template std::vector<std::map<uint_fast64_t, std::pair<storm::storage::BitVector, double>>>
AbstractionInformation<storm::dd::DdType::CUDD>::decodeChoicesToUpdateSuccessorMapping(std::set<storm::expressions::Variable> const& player2Variables,
                                                                                       storm::dd::Bdd<storm::dd::DdType::CUDD> const& choices) const;
template std::vector<std::map<uint_fast64_t, std::pair<storm::storage::BitVector, double>>>
AbstractionInformation<storm::dd::DdType::Sylvan>::decodeChoicesToUpdateSuccessorMapping(std::set<storm::expressions::Variable> const& player2Variables,
                                                                                         storm::dd::Bdd<storm::dd::DdType::Sylvan> const& choices) const;
template std::vector<std::map<uint_fast64_t, std::pair<storm::storage::BitVector, storm::RationalNumber>>>
AbstractionInformation<storm::dd::DdType::Sylvan>::decodeChoicesToUpdateSuccessorMapping(std::set<storm::expressions::Variable> const& player2Variables,
                                                                                         storm::dd::Bdd<storm::dd::DdType::Sylvan> const& choices) const;
}  // namespace abstraction
}  // namespace storm::gbar
