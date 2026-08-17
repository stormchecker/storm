#include "storm/models/symbolic/Model.h"

#include <boost/algorithm/string/join.hpp>

#include "storm/adapters/AddExpressionAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/utility/constants.h"
#include "storm/utility/dd.h"
#include "storm/utility/macros.h"

namespace storm {
namespace models {
namespace symbolic {
template<storm::dd::DdType Type, typename ValueType>
Model<Type, ValueType>::Model(storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager,
                              storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates,
                              storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
                              std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
                              std::set<storm::expressions::Variable> const& columnVariables,
                              std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                              std::map<std::string, storm::expressions::Expression> labelToExpressionMap,
                              std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : storm::models::Model<ValueType>(modelType),
      manager(manager),
      reachableStates(reachableStates),
      transitionMatrix(transitionMatrix),
      rowVariables(rowVariables),
      rowExpressionAdapter(rowExpressionAdapter),
      columnVariables(columnVariables),
      rowColumnMetaVariablePairs(rowColumnMetaVariablePairs),
      labelToExpressionMap(labelToExpressionMap),
      rewardModels(rewardModels) {
    this->labelToBddMap.emplace("init", initialStates);
    this->labelToBddMap.emplace("deadlock", deadlockStates);
}

template<storm::dd::DdType Type, typename ValueType>
Model<Type, ValueType>::Model(storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager,
                              storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates,
                              storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
                              std::set<storm::expressions::Variable> const& columnVariables,
                              std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                              std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap, std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : storm::models::Model<ValueType>(modelType),
      manager(manager),
      reachableStates(reachableStates),
      transitionMatrix(transitionMatrix),
      rowVariables(rowVariables),
      rowExpressionAdapter(nullptr),
      columnVariables(columnVariables),
      rowColumnMetaVariablePairs(rowColumnMetaVariablePairs),
      labelToBddMap(labelToBddMap),
      rewardModels(rewardModels) {
    STORM_LOG_THROW(this->labelToBddMap.find("init") == this->labelToBddMap.end(), storm::exceptions::WrongFormatException, "Illegal custom label 'init'.");
    STORM_LOG_THROW(this->labelToBddMap.find("deadlock") == this->labelToBddMap.end(), storm::exceptions::WrongFormatException,
                    "Illegal custom label 'deadlock'.");
    this->labelToBddMap.emplace("init", initialStates);
    this->labelToBddMap.emplace("deadlock", deadlockStates);
}

template<storm::dd::DdType Type, typename ValueType>
uint_fast64_t Model<Type, ValueType>::getNumberOfStates() const {
    return reachableStates.getNonZeroCount();
}

template<storm::dd::DdType Type, typename ValueType>
uint_fast64_t Model<Type, ValueType>::getNumberOfTransitions() const {
    return transitionMatrix.getNonZeroCount();
}

template<storm::dd::DdType Type, typename ValueType>
uint_fast64_t Model<Type, ValueType>::getNumberOfChoices() const {
    return reachableStates.getNonZeroCount();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::DdManager<Type>& Model<Type, ValueType>::getManager() const {
    return *manager;
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::dd::DdManager<Type>> const& Model<Type, ValueType>::getManagerAsSharedPointer() const {
    return manager;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& Model<Type, ValueType>::getReachableStates() const {
    return reachableStates;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& Model<Type, ValueType>::getInitialStates() const {
    return labelToBddMap.at("init");
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& Model<Type, ValueType>::getDeadlockStates() const {
    return labelToBddMap.at("deadlock");
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> Model<Type, ValueType>::getStates(std::string const& label) const {
    // First check whether we have a BDD for this label.
    auto bddIt = labelToBddMap.find(label);
    if (bddIt != labelToBddMap.end()) {
        return bddIt->second;
    } else {
        // If not, check for an expression we can translate.
        auto expressionIt = labelToExpressionMap.find(label);
        STORM_LOG_THROW(expressionIt != labelToExpressionMap.end(), storm::exceptions::IllegalArgumentException,
                        "The label " << label << " is invalid for the labeling of the model.");
        return this->getStates(expressionIt->second);
    }
}

template<storm::dd::DdType Type, typename ValueType>
storm::expressions::Expression Model<Type, ValueType>::getExpression(std::string const& label) const {
    auto expressionIt = labelToExpressionMap.find(label);
    STORM_LOG_THROW(expressionIt != labelToExpressionMap.end(), storm::exceptions::IllegalArgumentException,
                    "Cannot retrieve the expression for the label " << label << ".");
    return expressionIt->second;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> Model<Type, ValueType>::getStates(storm::expressions::Expression const& expression) const {
    if (expression.isTrue()) {
        return this->getReachableStates();
    } else if (expression.isFalse()) {
        return manager->getBddZero();
    }

    // Look up the string equivalent of the expression.
    std::stringstream stream;
    stream << expression;
    auto bddIt = labelToBddMap.find(stream.str());
    if (bddIt != labelToBddMap.end()) {
        return bddIt->second;
    }

    // Finally try to translate the expression with an adapter.
    STORM_LOG_THROW(rowExpressionAdapter != nullptr, storm::exceptions::InvalidOperationException,
                    "Cannot create BDD for expression without expression adapter.");
    return rowExpressionAdapter->translateExpression(expression).toBdd() && this->reachableStates;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::hasLabel(std::string const& label) const {
    auto bddIt = labelToBddMap.find(label);
    if (bddIt != labelToBddMap.end()) {
        return true;
    }

    auto expressionIt = labelToExpressionMap.find(label);
    if (expressionIt != labelToExpressionMap.end()) {
        return true;
    } else {
        return false;
    }
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> const& Model<Type, ValueType>::getTransitionMatrix() const {
    return transitionMatrix;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType>& Model<Type, ValueType>::getTransitionMatrix() {
    return transitionMatrix;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> Model<Type, ValueType>::getQualitativeTransitionMatrix(bool) const {
    return this->getTransitionMatrix().notZero();
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& Model<Type, ValueType>::getRowVariables() const {
    return rowVariables;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& Model<Type, ValueType>::getColumnVariables() const {
    return columnVariables;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> Model<Type, ValueType>::getRowAndNondeterminismVariables() const {
    std::set<storm::expressions::Variable> result;
    std::set_union(this->getRowVariables().begin(), this->getRowVariables().end(), this->getNondeterminismVariables().begin(),
                   this->getNondeterminismVariables().end(), std::inserter(result, result.begin()));
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> Model<Type, ValueType>::getColumnAndNondeterminismVariables() const {
    std::set<storm::expressions::Variable> result;
    std::set_union(this->getColumnVariables().begin(), this->getColumnVariables().end(), this->getNondeterminismVariables().begin(),
                   this->getNondeterminismVariables().end(), std::inserter(result, result.begin()));
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> const& Model<Type, ValueType>::getNondeterminismVariables() const {
    return emptyVariableSet;
}

template<storm::dd::DdType Type, typename ValueType>
std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& Model<Type, ValueType>::getRowColumnMetaVariablePairs() const {
    return rowColumnMetaVariablePairs;
}

template<storm::dd::DdType Type, typename ValueType>
std::map<std::string, storm::expressions::Expression> const& Model<Type, ValueType>::getLabelToExpressionMap() const {
    return labelToExpressionMap;
}

template<storm::dd::DdType Type, typename ValueType>
std::map<std::string, storm::dd::Bdd<Type>> const& Model<Type, ValueType>::getLabelToBddMap() const {
    return labelToBddMap;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> Model<Type, ValueType>::getRowColumnIdentity() const {
    return (storm::utility::dd::getRowColumnDiagonal<Type>(this->getManager(), this->getRowColumnMetaVariablePairs()) && this->getReachableStates())
        .template toAdd<ValueType>();
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::hasRewardModel(std::string const& rewardModelName) const {
    return this->rewardModels.find(rewardModelName) != this->rewardModels.end();
}

template<storm::dd::DdType Type, typename ValueType>
typename Model<Type, ValueType>::RewardModelType const& Model<Type, ValueType>::getRewardModel(std::string const& rewardModelName) const {
    auto it = this->rewardModels.find(rewardModelName);
    if (it == this->rewardModels.end()) {
        if (rewardModelName.empty()) {
            if (this->hasUniqueRewardModel()) {
                return this->getUniqueRewardModel();
            } else {
                STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException,
                                "Unable to refer to default reward model, because there is no default model or it is not unique.");
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "The requested reward model '" << rewardModelName << "' does not exist.");
        }
    }
    return it->second;
}

template<storm::dd::DdType Type, typename ValueType>
typename Model<Type, ValueType>::RewardModelType const& Model<Type, ValueType>::getUniqueRewardModel() const {
    STORM_LOG_THROW(this->hasUniqueRewardModel(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve unique reward model, because there is no unique one.");
    return this->rewardModels.cbegin()->second;
}

template<storm::dd::DdType Type, typename ValueType>
std::string const& Model<Type, ValueType>::getUniqueRewardModelName() const {
    STORM_LOG_THROW(this->hasUniqueRewardModel(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve name of unique reward model, because there is no unique one.");
    return this->rewardModels.cbegin()->first;
}

template<storm::dd::DdType Type, typename ValueType>
typename Model<Type, ValueType>::RewardModelType& Model<Type, ValueType>::getUniqueRewardModel() {
    STORM_LOG_THROW(this->hasUniqueRewardModel(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve unique reward model, because there is no unique one.");
    return this->rewardModels.begin()->second;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::hasUniqueRewardModel() const {
    return this->rewardModels.size() == 1;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::hasRewardModel() const {
    return !this->rewardModels.empty();
}

template<storm::dd::DdType Type, typename ValueType>
std::unordered_map<std::string, typename Model<Type, ValueType>::RewardModelType>& Model<Type, ValueType>::getRewardModels() {
    return this->rewardModels;
}

template<storm::dd::DdType Type, typename ValueType>
std::unordered_map<std::string, typename Model<Type, ValueType>::RewardModelType> const& Model<Type, ValueType>::getRewardModels() const {
    return this->rewardModels;
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::printModelInformationToStream(std::ostream& out) const {
    this->printModelInformationHeaderToStream(out);
    this->printModelInformationFooterToStream(out);
}

template<storm::dd::DdType Type, typename ValueType>
std::vector<std::string> Model<Type, ValueType>::getLabels() const {
    std::vector<std::string> labels;
    for (auto const& entry : labelToExpressionMap) {
        labels.push_back(entry.first);
    }
    return labels;
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::printModelInformationHeaderToStream(std::ostream& out) const {
    out << "-------------------------------------------------------------- \n";
    out << "Model type: \t" << (storm::IsIntervalType<ValueType> ? "I" : "") << this->getType() << " (symbolic)\n";
    out << "States: \t" << this->getNumberOfStates() << " (" << reachableStates.getNodeCount() << " nodes)\n";
    out << "Transitions: \t" << this->getNumberOfTransitions() << " (" << transitionMatrix.getNodeCount() << " nodes)\n";
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::printModelInformationFooterToStream(std::ostream& out) const {
    this->printRewardModelsInformationToStream(out);
    this->printDdVariableInformationToStream(out);
    out << "\nLabels: \t" << (this->labelToExpressionMap.size() + this->labelToBddMap.size()) << '\n';
    for (auto const& label : labelToBddMap) {
        out << "   * " << label.first << " -> " << label.second.getNonZeroCount() << " state(s) (" << label.second.getNodeCount() << " nodes)\n";
    }
    for (auto const& label : labelToExpressionMap) {
        out << "   * " << label.first << '\n';
    }
    out << "-------------------------------------------------------------- \n";
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::printRewardModelsInformationToStream(std::ostream& out) const {
    if (this->rewardModels.size()) {
        std::vector<std::string> rewardModelNames;
        std::for_each(this->rewardModels.cbegin(), this->rewardModels.cend(),
                      [&rewardModelNames](typename std::pair<std::string, RewardModelType> const& nameRewardModelPair) {
                          if (nameRewardModelPair.first.empty()) {
                              rewardModelNames.push_back("(default)");
                          } else {
                              rewardModelNames.push_back(nameRewardModelPair.first);
                          }
                      });
        out << "Reward Models:  " << boost::join(rewardModelNames, ", ") << '\n';
    } else {
        out << "Reward Models:  none\n";
    }
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::printDdVariableInformationToStream(std::ostream& out) const {
    uint_fast64_t rowVariableCount = 0;
    for (auto const& metaVariable : this->rowVariables) {
        rowVariableCount += this->getManager().getMetaVariable(metaVariable).getNumberOfDdVariables();
    }
    uint_fast64_t columnVariableCount = 0;
    for (auto const& metaVariable : this->columnVariables) {
        columnVariableCount += this->getManager().getMetaVariable(metaVariable).getNumberOfDdVariables();
    }

    out << "Variables: \t"
        << "rows: " << this->rowVariables.size() << " meta variables (" << rowVariableCount << " DD variables)"
        << ", columns: " << this->columnVariables.size() << " meta variables (" << columnVariableCount << " DD variables)";
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> const& Model<Type, ValueType>::getRowExpressionAdapter() const {
    return this->rowExpressionAdapter;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::isSymbolicModel() const {
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
std::optional<storm::dd::DdType> Model<Type, ValueType>::getDdType() const {
    return Type;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::isExact() const {
    return storm::NumberTraits<ValueType>::IsExact;
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::supportsParameters() const {
    return std::is_same<ValueType, storm::RationalFunction>::value;
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::writeDotToFile(std::string const& filename) const {
    this->getTransitionMatrix().exportToDot(filename, true);
    this->getInitialStates().exportToDot(filename, true);
    for (auto const& lab : this->getLabels()) {
        this->getStates(lab).exportToDot(filename, true);
    }
}

template<storm::dd::DdType Type, typename ValueType>
bool Model<Type, ValueType>::hasParameters() const {
    if (!this->supportsParameters()) {
        return false;
    }
    // Check for parameters
    for (auto it = this->getTransitionMatrix().begin(false); it != this->getTransitionMatrix().end(); ++it) {
        if (!storm::utility::isConstant((*it).second)) {
            return true;
        }
    }
    // Only constant values present
    return false;
}

template<storm::dd::DdType Type, typename ValueType>
void Model<Type, ValueType>::addParameters(std::set<storm::RationalFunctionVariable> const& parameters) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This value type does not support parameters.");
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::RationalFunctionVariable> const& Model<Type, ValueType>::getParameters() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This value type does not support parameters.");
}

template<>
void Model<storm::dd::DdType::Sylvan, storm::RationalFunction>::addParameters(std::set<storm::RationalFunctionVariable> const& parameters) {
    this->parameters.insert(parameters.begin(), parameters.end());
}

template<>
std::set<storm::RationalFunctionVariable> const& Model<storm::dd::DdType::Sylvan, storm::RationalFunction>::getParameters() const {
    return parameters;
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
typename std::enable_if<!std::is_same<ValueType, NewValueType>::value, std::shared_ptr<Model<Type, NewValueType>>>::type Model<Type, ValueType>::toValueType()
    const {
    STORM_LOG_TRACE("Converting value type of symbolic model from " << typeid(ValueType).name() << " to " << typeid(NewValueType).name() << ".");

    // Make a huge branching here as we cannot make a templated function virtual.
    if (this->getType() == storm::models::ModelType::Dtmc) {
        return this->template as<storm::models::symbolic::Dtmc<Type, ValueType>>()->template toValueType<NewValueType>();
    } else if (this->getType() == storm::models::ModelType::Ctmc) {
        return this->template as<storm::models::symbolic::Ctmc<Type, ValueType>>()->template toValueType<NewValueType>();
    } else if (this->getType() == storm::models::ModelType::Mdp) {
        return this->template as<storm::models::symbolic::Mdp<Type, ValueType>>()->template toValueType<NewValueType>();
    } else if (this->getType() == storm::models::ModelType::MarkovAutomaton) {
        return this->template as<storm::models::symbolic::MarkovAutomaton<Type, ValueType>>()->template toValueType<NewValueType>();
    } else if (this->getType() == storm::models::ModelType::S2pg) {
        return this->template as<storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType>>()->template toValueType<NewValueType>();
    }

    STORM_LOG_WARN("Could not convert value type of model.");
    return nullptr;
}

template<storm::dd::DdType Type, typename ValueType>
template<typename NewValueType>
typename std::enable_if<std::is_same<ValueType, NewValueType>::value, std::shared_ptr<Model<Type, NewValueType>>>::type Model<Type, ValueType>::toValueType()
    const {
    // Make a huge branching here as we cannot make a templated function virtual.
    if (this->getType() == storm::models::ModelType::Dtmc) {
        return std::make_shared<storm::models::symbolic::Dtmc<Type, ValueType>>(*this->template as<storm::models::symbolic::Dtmc<Type, ValueType>>());
    } else if (this->getType() == storm::models::ModelType::Ctmc) {
        return std::make_shared<storm::models::symbolic::Ctmc<Type, ValueType>>(*this->template as<storm::models::symbolic::Ctmc<Type, ValueType>>());
    } else if (this->getType() == storm::models::ModelType::Mdp) {
        return std::make_shared<storm::models::symbolic::Mdp<Type, ValueType>>(*this->template as<storm::models::symbolic::Mdp<Type, ValueType>>());
    } else if (this->getType() == storm::models::ModelType::MarkovAutomaton) {
        return std::make_shared<storm::models::symbolic::MarkovAutomaton<Type, ValueType>>(
            *this->template as<storm::models::symbolic::MarkovAutomaton<Type, ValueType>>());
    } else if (this->getType() == storm::models::ModelType::S2pg) {
        return std::make_shared<storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType>>(
            *this->template as<storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType>>());
    }

    STORM_LOG_WARN("Could not convert value type of model.");
    return nullptr;
}

// Explicitly instantiate the template class.
template class Model<storm::dd::DdType::CUDD, double>;
template typename std::enable_if<std::is_same<double, double>::value, std::shared_ptr<Model<storm::dd::DdType::CUDD, double>>>::type
Model<storm::dd::DdType::CUDD, double>::toValueType<double>() const;

template class Model<storm::dd::DdType::Sylvan, double>;
template class Model<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template typename std::enable_if<std::is_same<double, double>::value, std::shared_ptr<Model<storm::dd::DdType::Sylvan, double>>>::type
Model<storm::dd::DdType::Sylvan, double>::toValueType<double>() const;
template typename std::enable_if<std::is_same<storm::RationalNumber, storm::RationalNumber>::value,
                                 std::shared_ptr<Model<storm::dd::DdType::Sylvan, storm::RationalNumber>>>::type
Model<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType<storm::RationalNumber>() const;
template typename std::enable_if<std::is_same<storm::RationalFunction, storm::RationalFunction>::value,
                                 std::shared_ptr<Model<storm::dd::DdType::Sylvan, storm::RationalFunction>>>::type
Model<storm::dd::DdType::Sylvan, storm::RationalFunction>::toValueType<storm::RationalFunction>() const;
template typename std::enable_if<!std::is_same<storm::RationalNumber, double>::value, std::shared_ptr<Model<storm::dd::DdType::Sylvan, double>>>::type
Model<storm::dd::DdType::Sylvan, storm::RationalNumber>::toValueType<double>() const;
template class Model<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace symbolic
}  // namespace models
}  // namespace storm
