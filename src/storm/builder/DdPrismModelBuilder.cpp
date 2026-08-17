#include "storm/builder/DdPrismModelBuilder.h"

#include <boost/algorithm/string/join.hpp>

#include "storm/adapters/AddExpressionAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/prism/Compositions.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/dd.h"

namespace storm {
namespace builder {

template<storm::dd::DdType Type, typename ValueType>
class ParameterCreator {
   public:
    void create(storm::prism::Program const& /*program*/, storm::adapters::AddExpressionAdapter<Type, ValueType>& /*rowExpressionAdapter*/) {
        // Intentionally left empty: no support for parameters for this data type.
    }

    std::set<storm::RationalFunctionVariable> const& getParameters() const {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Creating parameters for non-parametric model is not supported.");
    }

   private:
};

template<storm::dd::DdType Type>
class ParameterCreator<Type, storm::RationalFunction> {
   public:
    ParameterCreator() : cache(std::make_shared<storm::RawPolynomialCache>()) {
        // Intentionally left empty.
    }

    void create(storm::prism::Program const& program, storm::adapters::AddExpressionAdapter<Type, storm::RationalFunction>& rowExpressionAdapter) {
        for (auto const& constant : program.getConstants()) {
            if (!constant.isDefined()) {
                storm::RationalFunctionVariable carlVariable = storm::createRFVariable(constant.getExpressionVariable().getName());
                parameters.insert(carlVariable);
                auto rf = convertVariableToPolynomial(carlVariable);
                rowExpressionAdapter.setValue(constant.getExpressionVariable(), rf);
            }
        }
    }

    template<typename RationalFunctionType = storm::RationalFunction, typename TP = typename RationalFunctionType::PolyType,
             carl::EnableIf<carl::needs_cache<TP>> = carl::dummy>
    RationalFunctionType convertVariableToPolynomial(storm::RationalFunctionVariable const& variable) {
        return RationalFunctionType(typename RationalFunctionType::PolyType(typename RationalFunctionType::PolyType::PolyType(variable), cache));
    }

    template<typename RationalFunctionType = storm::RationalFunction, typename TP = typename RationalFunctionType::PolyType,
             carl::DisableIf<carl::needs_cache<TP>> = carl::dummy>
    RationalFunctionType convertVariableToPolynomial(storm::RationalFunctionVariable const& variable) {
        return RationalFunctionType(variable);
    }

    std::set<storm::RationalFunctionVariable> const& getParameters() const {
        return parameters;
    }

   private:
    // A mapping from our variables to carl's.
    std::unordered_map<storm::expressions::Variable, storm::RationalFunctionVariable> variableToVariableMap;

    // The cache that is used in case the underlying type needs a cache.
    std::shared_ptr<storm::RawPolynomialCache> cache;

    // All created parameters.
    std::set<storm::RationalFunctionVariable> parameters;
};

template<storm::dd::DdType Type, typename ValueType>
class DdPrismModelBuilder<Type, ValueType>::GenerationInformation {
   public:
    GenerationInformation(storm::prism::Program const& program, std::shared_ptr<storm::dd::DdManager<Type>> const& manager)
        : program(program),
          manager(manager),
          rowMetaVariables(),
          variableToRowMetaVariableMap(std::make_shared<std::map<storm::expressions::Variable, storm::expressions::Variable>>()),
          rowExpressionAdapter(std::make_shared<storm::adapters::AddExpressionAdapter<Type, ValueType>>(manager, variableToRowMetaVariableMap)),
          columnMetaVariables(),
          variableToColumnMetaVariableMap((std::make_shared<std::map<storm::expressions::Variable, storm::expressions::Variable>>())),
          rowColumnMetaVariablePairs(),
          nondeterminismMetaVariables(),
          variableToIdentityMap(),
          allGlobalVariables(),
          moduleToIdentityMap(),
          parameters() {
        // Initializes variables and identity DDs.
        createMetaVariablesAndIdentities();

        // Initialize the parameters (if any).
        ParameterCreator<Type, ValueType> parameterCreator;
        parameterCreator.create(this->program, *this->rowExpressionAdapter);
        if (std::is_same<ValueType, storm::RationalFunction>::value) {
            this->parameters = parameterCreator.getParameters();
        }
    }

    // The program that is currently translated.
    storm::prism::Program const& program;

    // The manager used to build the decision diagrams.
    std::shared_ptr<storm::dd::DdManager<Type>> manager;

    // The meta variables for the row encoding.
    std::set<storm::expressions::Variable> rowMetaVariables;
    std::shared_ptr<std::map<storm::expressions::Variable, storm::expressions::Variable>> variableToRowMetaVariableMap;
    std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter;

    // The meta variables for the column encoding.
    std::set<storm::expressions::Variable> columnMetaVariables;
    std::shared_ptr<std::map<storm::expressions::Variable, storm::expressions::Variable>> variableToColumnMetaVariableMap;

    // All pairs of row/column meta variables.
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs;

    // The meta variables used to encode the nondeterminism.
    std::vector<storm::expressions::Variable> nondeterminismMetaVariables;

    // The meta variables used to encode the synchronization.
    std::vector<storm::expressions::Variable> synchronizationMetaVariables;

    // A set of all variables used for encoding the nondeterminism (i.e. nondetermism + synchronization
    // variables). This is handy to abstract from this variable set.
    std::set<storm::expressions::Variable> allNondeterminismVariables;

    // As set of all variables used for encoding the synchronization.
    std::set<storm::expressions::Variable> allSynchronizationMetaVariables;

    // DDs representing the identity for each variable.
    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> variableToIdentityMap;

    // A set of all meta variables that correspond to global variables.
    std::set<storm::expressions::Variable> allGlobalVariables;

    // DDs representing the identity for each module.
    std::map<std::string, storm::dd::Add<Type, ValueType>> moduleToIdentityMap;

    // DDs representing the valid ranges of the variables of each module.
    std::map<std::string, storm::dd::Add<Type, ValueType>> moduleToRangeMap;

    // The parameters appearing in the model.
    std::set<storm::RationalFunctionVariable> parameters;

   private:
    /*!
     * Creates the required meta variables and variable/module identities.
     */
    void createMetaVariablesAndIdentities() {
        // Add synchronization variables.
        for (auto const& actionIndex : program.getSynchronizingActionIndices()) {
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = manager->addMetaVariable(program.getActionName(actionIndex));
            synchronizationMetaVariables.push_back(variablePair.first);
            allSynchronizationMetaVariables.insert(variablePair.first);
            allNondeterminismVariables.insert(variablePair.first);
        }

        // Add nondeterminism variables (number of modules + number of commands).
        uint_fast64_t numberOfNondeterminismVariables = program.getModules().size();
        for (auto const& module : program.getModules()) {
            numberOfNondeterminismVariables += module.getNumberOfCommands();
        }
        for (uint_fast64_t i = 0; i < numberOfNondeterminismVariables; ++i) {
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = manager->addMetaVariable("nondet" + std::to_string(i));
            nondeterminismMetaVariables.push_back(variablePair.first);
            allNondeterminismVariables.insert(variablePair.first);
        }

        // Create meta variables for global program variables.
        for (storm::prism::IntegerVariable const& integerVariable : program.getGlobalIntegerVariables()) {
            int_fast64_t low = integerVariable.getLowerBoundExpression().evaluateAsInt();
            int_fast64_t high = integerVariable.getUpperBoundExpression().evaluateAsInt();
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = manager->addMetaVariable(integerVariable.getName(), low, high);

            STORM_LOG_TRACE("Created meta variables for global integer variable: " << variablePair.first.getName() << "[" << variablePair.first.getIndex()
                                                                                   << "] and " << variablePair.second.getName() << "["
                                                                                   << variablePair.second.getIndex() << "]");

            rowMetaVariables.insert(variablePair.first);
            variableToRowMetaVariableMap->emplace(integerVariable.getExpressionVariable(), variablePair.first);

            columnMetaVariables.insert(variablePair.second);
            variableToColumnMetaVariableMap->emplace(integerVariable.getExpressionVariable(), variablePair.second);

            storm::dd::Bdd<Type> variableIdentity = manager->getIdentity(variablePair.first, variablePair.second);
            variableToIdentityMap.emplace(integerVariable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());
            rowColumnMetaVariablePairs.push_back(variablePair);

            allGlobalVariables.insert(integerVariable.getExpressionVariable());
        }
        for (storm::prism::BooleanVariable const& booleanVariable : program.getGlobalBooleanVariables()) {
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = manager->addMetaVariable(booleanVariable.getName());

            STORM_LOG_TRACE("Created meta variables for global boolean variable: " << variablePair.first.getName() << "[" << variablePair.first.getIndex()
                                                                                   << "] and " << variablePair.second.getName() << "["
                                                                                   << variablePair.second.getIndex() << "]");

            rowMetaVariables.insert(variablePair.first);
            variableToRowMetaVariableMap->emplace(booleanVariable.getExpressionVariable(), variablePair.first);

            columnMetaVariables.insert(variablePair.second);
            variableToColumnMetaVariableMap->emplace(booleanVariable.getExpressionVariable(), variablePair.second);

            storm::dd::Bdd<Type> variableIdentity = manager->getIdentity(variablePair.first, variablePair.second);
            variableToIdentityMap.emplace(booleanVariable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());

            rowColumnMetaVariablePairs.push_back(variablePair);
            allGlobalVariables.insert(booleanVariable.getExpressionVariable());
        }

        // Create meta variables for each of the modules' variables.
        for (storm::prism::Module const& module : program.getModules()) {
            storm::dd::Bdd<Type> moduleIdentity = manager->getBddOne();
            storm::dd::Bdd<Type> moduleRange = manager->getBddOne();

            for (storm::prism::IntegerVariable const& integerVariable : module.getIntegerVariables()) {
                int_fast64_t low = integerVariable.getLowerBoundExpression().evaluateAsInt();
                int_fast64_t high = integerVariable.getUpperBoundExpression().evaluateAsInt();
                std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair =
                    manager->addMetaVariable(integerVariable.getName(), low, high);
                STORM_LOG_TRACE("Created meta variables for integer variable: " << variablePair.first.getName() << "[" << variablePair.first.getIndex()
                                                                                << "] and " << variablePair.second.getName() << "["
                                                                                << variablePair.second.getIndex() << "]");

                rowMetaVariables.insert(variablePair.first);
                variableToRowMetaVariableMap->emplace(integerVariable.getExpressionVariable(), variablePair.first);

                columnMetaVariables.insert(variablePair.second);
                variableToColumnMetaVariableMap->emplace(integerVariable.getExpressionVariable(), variablePair.second);

                storm::dd::Bdd<Type> variableIdentity = manager->getIdentity(variablePair.first, variablePair.second);
                variableToIdentityMap.emplace(integerVariable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());
                moduleIdentity &= variableIdentity;
                moduleRange &= manager->getRange(variablePair.first);

                rowColumnMetaVariablePairs.push_back(variablePair);
            }
            for (storm::prism::BooleanVariable const& booleanVariable : module.getBooleanVariables()) {
                std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = manager->addMetaVariable(booleanVariable.getName());
                STORM_LOG_TRACE("Created meta variables for boolean variable: " << variablePair.first.getName() << "[" << variablePair.first.getIndex()
                                                                                << "] and " << variablePair.second.getName() << "["
                                                                                << variablePair.second.getIndex() << "]");

                rowMetaVariables.insert(variablePair.first);
                variableToRowMetaVariableMap->emplace(booleanVariable.getExpressionVariable(), variablePair.first);

                columnMetaVariables.insert(variablePair.second);
                variableToColumnMetaVariableMap->emplace(booleanVariable.getExpressionVariable(), variablePair.second);

                storm::dd::Bdd<Type> variableIdentity = manager->getIdentity(variablePair.first, variablePair.second);
                variableToIdentityMap.emplace(booleanVariable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());
                moduleIdentity &= variableIdentity;
                moduleRange &= manager->getRange(variablePair.first);

                rowColumnMetaVariablePairs.push_back(variablePair);
            }
            moduleToIdentityMap[module.getName()] = moduleIdentity.template toAdd<ValueType>();
            moduleToRangeMap[module.getName()] = moduleRange.template toAdd<ValueType>();
        }
    }
};

template<storm::dd::DdType Type, typename ValueType>
class ModuleComposer : public storm::prism::CompositionVisitor {
   public:
    ModuleComposer(typename DdPrismModelBuilder<Type, ValueType>::GenerationInformation& generationInfo) : generationInfo(generationInfo) {
        // Intentionally left empty.
    }

    typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram compose(storm::prism::Composition const& composition) {
        return boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(
            composition.accept(*this, newSynchronizingActionToOffsetMap()));
    }

    std::map<uint_fast64_t, uint_fast64_t> newSynchronizingActionToOffsetMap() const {
        std::map<uint_fast64_t, uint_fast64_t> result;
        for (auto const& actionIndex : generationInfo.program.getSynchronizingActionIndices()) {
            result[actionIndex] = 0;
        }
        return result;
    }

    std::map<uint_fast64_t, uint_fast64_t> updateSynchronizingActionToOffsetMap(typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram const& sub,
                                                                                std::map<uint_fast64_t, uint_fast64_t> const& oldMapping) const {
        std::map<uint_fast64_t, uint_fast64_t> result = oldMapping;
        for (auto const& action : sub.synchronizingActionToDecisionDiagramMap) {
            result[action.first] = action.second.numberOfUsedNondeterminismVariables;
        }
        return result;
    }

    virtual boost::any visit(storm::prism::ModuleComposition const& composition, boost::any const& data) override {
        STORM_LOG_TRACE("Translating module '" << composition.getModuleName() << "'.");
        std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap = boost::any_cast<std::map<uint_fast64_t, uint_fast64_t> const&>(data);

        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram result = DdPrismModelBuilder<Type, ValueType>::createModuleDecisionDiagram(
            generationInfo, generationInfo.program.getModule(composition.getModuleName()), synchronizingActionToOffsetMap);

        return result;
    }

    virtual boost::any visit(storm::prism::RenamingComposition const& composition, boost::any const& data) override {
        // Create the mapping from action indices to action indices.
        std::map<uint_fast64_t, uint_fast64_t> renaming;
        for (auto const& namePair : composition.getActionRenaming()) {
            STORM_LOG_THROW(generationInfo.program.hasAction(namePair.first), storm::exceptions::InvalidArgumentException,
                            "Composition refers to unknown action '" << namePair.first << "'.");
            STORM_LOG_THROW(generationInfo.program.hasAction(namePair.second), storm::exceptions::InvalidArgumentException,
                            "Composition refers to unknown action '" << namePair.second << "'.");
            renaming.emplace(generationInfo.program.getActionIndex(namePair.first), generationInfo.program.getActionIndex(namePair.second));
        }

        // Prepare the new offset mapping.
        std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap = boost::any_cast<std::map<uint_fast64_t, uint_fast64_t> const&>(data);
        std::map<uint_fast64_t, uint_fast64_t> newSynchronizingActionToOffsetMap = synchronizingActionToOffsetMap;
        for (auto const& indexPair : renaming) {
            auto it = synchronizingActionToOffsetMap.find(indexPair.second);
            STORM_LOG_THROW(it != synchronizingActionToOffsetMap.end(), storm::exceptions::InvalidArgumentException,
                            "Invalid action index " << indexPair.second << ".");
            newSynchronizingActionToOffsetMap[indexPair.first] = it->second;
        }

        // Then, we translate the subcomposition.
        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram sub =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(
                composition.getSubcomposition().accept(*this, newSynchronizingActionToOffsetMap));

        // Perform the renaming and return result.
        return rename(sub, renaming);
    }

    virtual boost::any visit(storm::prism::HidingComposition const& composition, boost::any const& data) override {
        // Create the mapping from action indices to action indices.
        std::set<uint_fast64_t> actionIndicesToHide;
        for (auto const& action : composition.getActionsToHide()) {
            STORM_LOG_THROW(generationInfo.program.hasAction(action), storm::exceptions::InvalidArgumentException,
                            "Composition refers to unknown action '" << action << "'.");
            actionIndicesToHide.insert(generationInfo.program.getActionIndex(action));
        }

        // Prepare the new offset mapping.
        std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap = boost::any_cast<std::map<uint_fast64_t, uint_fast64_t> const&>(data);
        std::map<uint_fast64_t, uint_fast64_t> newSynchronizingActionToOffsetMap = synchronizingActionToOffsetMap;
        for (auto const& index : actionIndicesToHide) {
            newSynchronizingActionToOffsetMap[index] = 0;
        }

        // Then, we translate the subcomposition.
        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram sub =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(
                composition.getSubcomposition().accept(*this, newSynchronizingActionToOffsetMap));

        // Perform the hiding and return result.
        hide(sub, actionIndicesToHide);
        return sub;
    }

    virtual boost::any visit(storm::prism::SynchronizingParallelComposition const& composition, boost::any const& data) override {
        // First, we translate the subcompositions.
        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram left =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(composition.getLeftSubcomposition().accept(*this, data));

        // Prepare the new offset mapping.
        std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap = boost::any_cast<std::map<uint_fast64_t, uint_fast64_t> const&>(data);
        std::map<uint_fast64_t, uint_fast64_t> newSynchronizingActionToOffsetMap = synchronizingActionToOffsetMap;
        for (auto const& action : left.synchronizingActionToDecisionDiagramMap) {
            newSynchronizingActionToOffsetMap[action.first] = action.second.numberOfUsedNondeterminismVariables;
        }

        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram right =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(
                composition.getRightSubcomposition().accept(*this, newSynchronizingActionToOffsetMap));

        // Then, determine the action indices on which we need to synchronize.
        std::set<uint_fast64_t> leftSynchronizationActionIndices = left.getSynchronizingActionIndices();
        std::set<uint_fast64_t> rightSynchronizationActionIndices = right.getSynchronizingActionIndices();
        std::set<uint_fast64_t> synchronizationActionIndices;
        std::set_intersection(leftSynchronizationActionIndices.begin(), leftSynchronizationActionIndices.end(), rightSynchronizationActionIndices.begin(),
                              rightSynchronizationActionIndices.end(), std::inserter(synchronizationActionIndices, synchronizationActionIndices.begin()));

        // Finally, we compose the subcompositions to create the result.
        composeInParallel(left, right, synchronizationActionIndices);
        return left;
    }

    virtual boost::any visit(storm::prism::InterleavingParallelComposition const& composition, boost::any const& data) override {
        // First, we translate the subcompositions.
        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram left =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(composition.getLeftSubcomposition().accept(*this, data));

        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram right =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(composition.getRightSubcomposition().accept(*this, data));

        // Finally, we compose the subcompositions to create the result.
        composeInParallel(left, right, std::set<uint_fast64_t>());
        return left;
    }

    virtual boost::any visit(storm::prism::RestrictedParallelComposition const& composition, boost::any const& data) override {
        // Construct the synchronizing action indices from the synchronizing action names.
        std::set<uint_fast64_t> synchronizingActionIndices;
        for (auto const& action : composition.getSynchronizingActions()) {
            synchronizingActionIndices.insert(generationInfo.program.getActionIndex(action));
        }

        // Then, we translate the subcompositions.
        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram left =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(composition.getLeftSubcomposition().accept(*this, data));

        // Prepare the new offset mapping.
        std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap = boost::any_cast<std::map<uint_fast64_t, uint_fast64_t> const&>(data);
        std::map<uint_fast64_t, uint_fast64_t> newSynchronizingActionToOffsetMap = synchronizingActionToOffsetMap;
        for (auto const& actionIndex : synchronizingActionIndices) {
            auto it = left.synchronizingActionToDecisionDiagramMap.find(actionIndex);
            if (it != left.synchronizingActionToDecisionDiagramMap.end()) {
                newSynchronizingActionToOffsetMap[actionIndex] = it->second.numberOfUsedNondeterminismVariables;
            }
        }

        typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram right =
            boost::any_cast<typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram>(
                composition.getRightSubcomposition().accept(*this, newSynchronizingActionToOffsetMap));

        std::set<uint_fast64_t> leftSynchronizationActionIndices = left.getSynchronizingActionIndices();
        bool isContainedInLeft = std::includes(leftSynchronizationActionIndices.begin(), leftSynchronizationActionIndices.end(),
                                               synchronizingActionIndices.begin(), synchronizingActionIndices.end());
        STORM_LOG_WARN_COND(isContainedInLeft,
                            "Left subcomposition of composition '" << composition << "' does not include all actions over which to synchronize.");

        std::set<uint_fast64_t> rightSynchronizationActionIndices = right.getSynchronizingActionIndices();
        bool isContainedInRight = std::includes(rightSynchronizationActionIndices.begin(), rightSynchronizationActionIndices.end(),
                                                synchronizingActionIndices.begin(), synchronizingActionIndices.end());
        STORM_LOG_WARN_COND(isContainedInRight,
                            "Right subcomposition of composition '" << composition << "' does not include all actions over which to synchronize.");

        // Finally, we compose the subcompositions to create the result.
        composeInParallel(left, right, synchronizingActionIndices);
        return left;
    }

   private:
    /*!
     * Hides the actions of the given module according to the given set. As a result, the module is modified in
     * place.
     */
    void hide(typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram& sub, std::set<uint_fast64_t> const& actionIndicesToHide) const {
        STORM_LOG_TRACE("Hiding actions.");

        for (auto const& actionIndex : actionIndicesToHide) {
            auto it = sub.synchronizingActionToDecisionDiagramMap.find(actionIndex);
            if (it != sub.synchronizingActionToDecisionDiagramMap.end()) {
                sub.independentAction = DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(generationInfo, sub.independentAction, it->second);
                sub.numberOfUsedNondeterminismVariables =
                    std::max(sub.numberOfUsedNondeterminismVariables, sub.independentAction.numberOfUsedNondeterminismVariables);
                sub.synchronizingActionToDecisionDiagramMap.erase(it);
            }
        }
    }

    /*!
     * Renames the actions of the given module according to the given renaming.
     */
    typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram rename(typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram& sub,
                                                                                std::map<uint_fast64_t, uint_fast64_t> const& renaming) const {
        STORM_LOG_TRACE("Renaming actions.");
        std::map<uint_fast64_t, typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram> actionIndexToDdMap;

        // Go through all action DDs with a synchronizing label and rename them if they appear in the renaming.
        for (auto& action : sub.synchronizingActionToDecisionDiagramMap) {
            auto renamingIt = renaming.find(action.first);
            if (renamingIt != renaming.end()) {
                // If the action is to be renamed and an action with the target index already exists, we need
                // to combine the action DDs.
                auto itNewActions = actionIndexToDdMap.find(renamingIt->second);
                if (itNewActions != actionIndexToDdMap.end()) {
                    actionIndexToDdMap[renamingIt->second] =
                        DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(generationInfo, action.second, itNewActions->second);

                } else {
                    // In this case, we can simply copy the action over.
                    actionIndexToDdMap[renamingIt->second] = action.second;
                }
            } else {
                // If the action is not to be renamed, we need to copy it over. However, if some other action
                // was renamed to the very same action name before, we need to combine the transitions.
                auto itNewActions = actionIndexToDdMap.find(action.first);
                if (itNewActions != actionIndexToDdMap.end()) {
                    actionIndexToDdMap[action.first] =
                        DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(generationInfo, action.second, itNewActions->second);
                } else {
                    // In this case, we can simply copy the action over.
                    actionIndexToDdMap[action.first] = action.second;
                }
            }
        }

        return typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram(sub.independentAction, actionIndexToDdMap, sub.identity,
                                                                                    sub.numberOfUsedNondeterminismVariables);
    }

    /*!
     * Composes the given modules while synchronizing over the provided action indices. As a result, the first
     * module is modified in place and will contain the composition after a call to this method.
     */
    void composeInParallel(typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram& left,
                           typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram& right,
                           std::set<uint_fast64_t> const& synchronizationActionIndices) const {
        STORM_LOG_TRACE("Composing two modules.");

        // Combine the tau action.
        uint_fast64_t numberOfUsedNondeterminismVariables = right.independentAction.numberOfUsedNondeterminismVariables;
        left.independentAction = DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(generationInfo, left.independentAction,
                                                                                                    right.independentAction, left.identity, right.identity);
        numberOfUsedNondeterminismVariables = std::max(numberOfUsedNondeterminismVariables, left.independentAction.numberOfUsedNondeterminismVariables);

        // Create an empty action for the case where one of the modules does not have a certain action.
        typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram emptyAction(*generationInfo.manager);

        // Treat all non-tau actions of the left module.
        for (auto& action : left.synchronizingActionToDecisionDiagramMap) {
            // If we need to synchronize over this action index, we try to do so now.
            if (synchronizationActionIndices.find(action.first) != synchronizationActionIndices.end()) {
                // If we are to synchronize over an action that does not exist in the second module, the result
                // is that the synchronization is the empty action.
                if (!right.hasSynchronizingAction(action.first)) {
                    action.second = emptyAction;
                } else {
                    // Otherwise, the actions of the modules are synchronized.
                    action.second = DdPrismModelBuilder<Type, ValueType>::combineSynchronizingActions(
                        action.second, right.synchronizingActionToDecisionDiagramMap[action.first]);
                }
            } else {
                // If we don't synchronize over this action, we need to construct the interleaving.

                // If both modules contain the action, we need to mutually multiply the other identity.
                if (right.hasSynchronizingAction(action.first)) {
                    action.second = DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(
                        generationInfo, action.second, right.synchronizingActionToDecisionDiagramMap[action.first], left.identity, right.identity);
                } else {
                    // If only the first module has this action, we need to use a dummy action decision diagram
                    // for the second module.
                    action.second = DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(generationInfo, action.second, emptyAction,
                                                                                                       left.identity, right.identity);
                }
            }
            numberOfUsedNondeterminismVariables = std::max(numberOfUsedNondeterminismVariables, action.second.numberOfUsedNondeterminismVariables);
        }

        // Treat all non-tau actions of the right module.
        for (auto const& actionIndex : right.getSynchronizingActionIndices()) {
            // Here, we only need to treat actions that the first module does not have, because we have handled
            // this case earlier.
            if (!left.hasSynchronizingAction(actionIndex)) {
                if (synchronizationActionIndices.find(actionIndex) != synchronizationActionIndices.end()) {
                    // If we are to synchronize over this action that does not exist in the first module, the
                    // result is that the synchronization is the empty action.
                    left.synchronizingActionToDecisionDiagramMap[actionIndex] = emptyAction;
                } else {
                    // If only the second module has this action, we need to use a dummy action decision diagram
                    // for the first module.
                    left.synchronizingActionToDecisionDiagramMap[actionIndex] = DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(
                        generationInfo, emptyAction, right.synchronizingActionToDecisionDiagramMap[actionIndex], left.identity, right.identity);
                }
            }
            numberOfUsedNondeterminismVariables =
                std::max(numberOfUsedNondeterminismVariables, left.synchronizingActionToDecisionDiagramMap[actionIndex].numberOfUsedNondeterminismVariables);
        }

        // Combine identity matrices.
        left.identity = left.identity * right.identity;

        // Keep track of the number of nondeterminism variables used.
        left.numberOfUsedNondeterminismVariables = std::max(left.numberOfUsedNondeterminismVariables, numberOfUsedNondeterminismVariables);
    }

    typename DdPrismModelBuilder<Type, ValueType>::GenerationInformation& generationInfo;
};

template<storm::dd::DdType Type, typename ValueType>
bool DdPrismModelBuilder<Type, ValueType>::canHandle(storm::prism::Program const& program) {
    return !program.hasUnboundedVariables() && (program.getModelType() != storm::prism::Program::ModelType::PTA);
}

template<storm::dd::DdType Type, typename ValueType>
DdPrismModelBuilder<Type, ValueType>::Options::Options()
    : buildAllRewardModels(false), rewardModelsToBuild(), buildAllLabels(false), labelsToBuild(), terminalStates() {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
DdPrismModelBuilder<Type, ValueType>::Options::Options(storm::logic::Formula const& formula)
    : buildAllRewardModels(false), rewardModelsToBuild(), buildAllLabels(false), labelsToBuild(std::set<std::string>()) {
    this->preserveFormula(formula);
    this->setTerminalStatesFromFormula(formula);
}

template<storm::dd::DdType Type, typename ValueType>
DdPrismModelBuilder<Type, ValueType>::Options::Options(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas)
    : buildAllRewardModels(false), rewardModelsToBuild(), buildAllLabels(false), labelsToBuild() {
    for (auto const& formula : formulas) {
        this->preserveFormula(*formula);
    }
    if (formulas.size() == 1) {
        this->setTerminalStatesFromFormula(*formulas.front());
    }
}

template<storm::dd::DdType Type, typename ValueType>
void DdPrismModelBuilder<Type, ValueType>::Options::preserveFormula(storm::logic::Formula const& formula) {
    // If we already had terminal states, we need to erase them.
    terminalStates.clear();

    // If we are not required to build all reward models, we determine the reward models we need to build.
    if (!buildAllRewardModels) {
        std::set<std::string> referencedRewardModels = formula.getReferencedRewardModels();
        rewardModelsToBuild.insert(referencedRewardModels.begin(), referencedRewardModels.end());
    }

    // Extract all the labels used in the formula.
    std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> atomicLabelFormulas = formula.getAtomicLabelFormulas();
    for (auto const& formula : atomicLabelFormulas) {
        if (!labelsToBuild) {
            labelsToBuild = std::set<std::string>();
        }
        labelsToBuild.get().insert(formula->getLabel());
    }
}

template<storm::dd::DdType Type, typename ValueType>
void DdPrismModelBuilder<Type, ValueType>::Options::setTerminalStatesFromFormula(storm::logic::Formula const& formula) {
    terminalStates = getTerminalStatesFromFormula(formula);
}

template<storm::dd::DdType Type, typename ValueType>
struct DdPrismModelBuilder<Type, ValueType>::SystemResult {
    SystemResult(storm::dd::Add<Type, ValueType> const& allTransitionsDd, DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram const& globalModule,
                 boost::optional<storm::dd::Add<Type, ValueType>> const& stateActionDd)
        : allTransitionsDd(allTransitionsDd), globalModule(globalModule), stateActionDd(stateActionDd) {
        // Intentionally left empty.
    }

    storm::dd::Add<Type, ValueType> allTransitionsDd;
    typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram globalModule;
    boost::optional<storm::dd::Add<Type, ValueType>> stateActionDd;
};

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::UpdateDecisionDiagram DdPrismModelBuilder<Type, ValueType>::createUpdateDecisionDiagram(
    GenerationInformation& generationInfo, storm::prism::Module const& module, storm::dd::Add<Type, ValueType> const& guard,
    storm::prism::Update const& update) {
    storm::dd::Add<Type, ValueType> updateDd = generationInfo.manager->template getAddOne<ValueType>();

    STORM_LOG_TRACE("Translating update " << update);

    // Iterate over all assignments (boolean and integer) and build the DD for it.
    std::vector<storm::prism::Assignment> assignments = update.getAssignments();
    std::set<storm::expressions::Variable> assignedVariables;
    for (auto const& assignment : assignments) {
        // Record the variable as being written.
        STORM_LOG_TRACE("Assigning to variable " << generationInfo.variableToRowMetaVariableMap->at(assignment.getVariable()).getName());
        assignedVariables.insert(assignment.getVariable());

        // Translate the written variable.
        auto const& primedMetaVariable = generationInfo.variableToColumnMetaVariableMap->at(assignment.getVariable());
        storm::dd::Add<Type, ValueType> writtenVariable = generationInfo.manager->template getIdentity<ValueType>(primedMetaVariable);

        // Translate the expression that is being assigned.
        storm::dd::Add<Type, ValueType> updateExpression = generationInfo.rowExpressionAdapter->translateExpression(assignment.getExpression());

        // Combine the update expression with the guard.
        storm::dd::Add<Type, ValueType> result = updateExpression * guard;

        // Combine the variable and the assigned expression.
        storm::dd::Add<Type, ValueType> tmp = result;
        result = result.equals(writtenVariable).template toAdd<ValueType>();
        result *= guard;

        // Restrict the transitions to the range of the written variable.
        result = result * generationInfo.manager->getRange(primedMetaVariable).template toAdd<ValueType>();

        updateDd *= result;
    }

    // Compute the set of assigned global variables.
    std::set<storm::expressions::Variable> assignedGlobalVariables;
    std::set_intersection(assignedVariables.begin(), assignedVariables.end(), generationInfo.allGlobalVariables.begin(),
                          generationInfo.allGlobalVariables.end(), std::inserter(assignedGlobalVariables, assignedGlobalVariables.begin()));

    // All unassigned boolean variables need to keep their value.
    for (storm::prism::BooleanVariable const& booleanVariable : module.getBooleanVariables()) {
        if (assignedVariables.find(booleanVariable.getExpressionVariable()) == assignedVariables.end()) {
            STORM_LOG_TRACE("Multiplying identity of variable " << booleanVariable.getName());
            updateDd *= generationInfo.variableToIdentityMap.at(booleanVariable.getExpressionVariable());
        }
    }

    // All unassigned integer variables need to keep their value.
    for (storm::prism::IntegerVariable const& integerVariable : module.getIntegerVariables()) {
        if (assignedVariables.find(integerVariable.getExpressionVariable()) == assignedVariables.end()) {
            STORM_LOG_TRACE("Multiplying identity of variable " << integerVariable.getName());
            updateDd *= generationInfo.variableToIdentityMap.at(integerVariable.getExpressionVariable());
        }
    }

    return UpdateDecisionDiagram(updateDd, assignedGlobalVariables);
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::createCommandDecisionDiagram(
    GenerationInformation& generationInfo, storm::prism::Module const& module, storm::prism::Command const& command) {
    STORM_LOG_TRACE("Translating guard " << command.getGuardExpression());
    storm::dd::Bdd<Type> guard = generationInfo.rowExpressionAdapter->translateBooleanExpression(command.getGuardExpression()) &&
                                 generationInfo.moduleToRangeMap[module.getName()].notZero();
    STORM_LOG_WARN_COND(!guard.isZero(), "The guard '" << command.getGuardExpression() << "' is unsatisfiable.");

    if (!guard.isZero()) {
        // Create the DDs representing the individual updates.
        std::vector<UpdateDecisionDiagram> updateResults;
        for (storm::prism::Update const& update : command.getUpdates()) {
            updateResults.push_back(createUpdateDecisionDiagram(generationInfo, module, guard.template toAdd<ValueType>(), update));

            STORM_LOG_WARN_COND(!updateResults.back().updateDd.isZero(), "Update '" << update << "' does not have any effect.");
        }

        // Start by gathering all variables that were written in at least one update.
        std::set<storm::expressions::Variable> globalVariablesInSomeUpdate;

        // If the command is labeled, we have to analyze which portion of the global variables was written by
        // any of the updates and make all update results equal w.r.t. this set. If the command is not labeled,
        // we can already multiply the identities of all global variables.
        if (command.isLabeled()) {
            std::for_each(updateResults.begin(), updateResults.end(), [&globalVariablesInSomeUpdate](UpdateDecisionDiagram const& update) {
                globalVariablesInSomeUpdate.insert(update.assignedGlobalVariables.begin(), update.assignedGlobalVariables.end());
            });
        } else {
            globalVariablesInSomeUpdate = generationInfo.allGlobalVariables;
        }

        // Then, multiply the missing identities.
        for (auto& updateResult : updateResults) {
            std::set<storm::expressions::Variable> missingIdentities;
            std::set_difference(globalVariablesInSomeUpdate.begin(), globalVariablesInSomeUpdate.end(), updateResult.assignedGlobalVariables.begin(),
                                updateResult.assignedGlobalVariables.end(), std::inserter(missingIdentities, missingIdentities.begin()));

            for (auto const& variable : missingIdentities) {
                STORM_LOG_TRACE("Multiplying identity for variable " << variable.getName() << "[" << variable.getIndex() << "] to update.");
                updateResult.updateDd *= generationInfo.variableToIdentityMap.at(variable);
            }
        }

        // Now combine the update DDs to the command DD.
        storm::dd::Add<Type, ValueType> commandDd = generationInfo.manager->template getAddZero<ValueType>();
        auto updateResultsIt = updateResults.begin();
        for (auto updateIt = command.getUpdates().begin(), updateIte = command.getUpdates().end(); updateIt != updateIte; ++updateIt, ++updateResultsIt) {
            storm::dd::Add<Type, ValueType> probabilityDd = generationInfo.rowExpressionAdapter->translateExpression(updateIt->getLikelihoodExpression());
            commandDd += updateResultsIt->updateDd * probabilityDd;
        }

        return ActionDecisionDiagram(guard, guard.template toAdd<ValueType>() * commandDd, globalVariablesInSomeUpdate);
    } else {
        return ActionDecisionDiagram(*generationInfo.manager);
    }
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::createActionDecisionDiagram(
    GenerationInformation& generationInfo, storm::prism::Module const& module, uint_fast64_t synchronizationActionIndex,
    uint_fast64_t nondeterminismVariableOffset) {
    std::vector<ActionDecisionDiagram> commandDds;
    for (storm::prism::Command const& command : module.getCommands()) {
        // Determine whether the command is relevant for the selected action.
        bool relevant = (synchronizationActionIndex == 0 && !command.isLabeled()) ||
                        (synchronizationActionIndex && command.isLabeled() && command.getActionIndex() == synchronizationActionIndex);

        if (!relevant) {
            continue;
        }

        STORM_LOG_TRACE("Translating command " << command);

        // At this point, the command is known to be relevant for the action.
        commandDds.push_back(createCommandDecisionDiagram(generationInfo, module, command));
    }

    ActionDecisionDiagram result(*generationInfo.manager);
    if (!commandDds.empty()) {
        switch (generationInfo.program.getModelType()) {
            case storm::prism::Program::ModelType::DTMC:
            case storm::prism::Program::ModelType::CTMC:
                result = combineCommandsToActionMarkovChain(generationInfo, commandDds);
                break;
            case storm::prism::Program::ModelType::MDP:
                result = combineCommandsToActionMDP(generationInfo, commandDds, nondeterminismVariableOffset);
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Cannot translate model of this type.");
        }
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> DdPrismModelBuilder<Type, ValueType>::equalizeAssignedGlobalVariables(GenerationInformation const& generationInfo,
                                                                                                             ActionDecisionDiagram& action1,
                                                                                                             ActionDecisionDiagram& action2) {
    // Start by gathering all variables that were written in at least one action DD.
    std::set<storm::expressions::Variable> globalVariablesInActionDd;
    std::set_union(action1.assignedGlobalVariables.begin(), action1.assignedGlobalVariables.end(), action2.assignedGlobalVariables.begin(),
                   action2.assignedGlobalVariables.end(), std::inserter(globalVariablesInActionDd, globalVariablesInActionDd.begin()));

    std::set<storm::expressions::Variable> missingIdentitiesInAction1;
    std::set_difference(globalVariablesInActionDd.begin(), globalVariablesInActionDd.end(), action1.assignedGlobalVariables.begin(),
                        action1.assignedGlobalVariables.end(), std::inserter(missingIdentitiesInAction1, missingIdentitiesInAction1.begin()));
    for (auto const& variable : missingIdentitiesInAction1) {
        action1.transitionsDd *= generationInfo.variableToIdentityMap.at(variable);
    }

    std::set<storm::expressions::Variable> missingIdentitiesInAction2;
    std::set_difference(globalVariablesInActionDd.begin(), globalVariablesInActionDd.end(), action1.assignedGlobalVariables.begin(),
                        action1.assignedGlobalVariables.end(), std::inserter(missingIdentitiesInAction2, missingIdentitiesInAction2.begin()));
    for (auto const& variable : missingIdentitiesInAction2) {
        action2.transitionsDd *= generationInfo.variableToIdentityMap.at(variable);
    }

    return globalVariablesInActionDd;
}

template<storm::dd::DdType Type, typename ValueType>
std::set<storm::expressions::Variable> DdPrismModelBuilder<Type, ValueType>::equalizeAssignedGlobalVariables(GenerationInformation const& generationInfo,
                                                                                                             std::vector<ActionDecisionDiagram>& actionDds) {
    // Start by gathering all variables that were written in at least one action DD.
    std::set<storm::expressions::Variable> globalVariablesInActionDd;
    for (auto const& commandDd : actionDds) {
        globalVariablesInActionDd.insert(commandDd.assignedGlobalVariables.begin(), commandDd.assignedGlobalVariables.end());
    }

    STORM_LOG_TRACE("Equalizing assigned global variables.");

    // Then multiply the transitions of each action with the missing identities.
    for (auto& actionDd : actionDds) {
        STORM_LOG_TRACE("Equalizing next action.");
        std::set<storm::expressions::Variable> missingIdentities;
        std::set_difference(globalVariablesInActionDd.begin(), globalVariablesInActionDd.end(), actionDd.assignedGlobalVariables.begin(),
                            actionDd.assignedGlobalVariables.end(), std::inserter(missingIdentities, missingIdentities.begin()));
        for (auto const& variable : missingIdentities) {
            STORM_LOG_TRACE("Multiplying identity of variable " << variable.getName() << ".");
            actionDd.transitionsDd *= generationInfo.variableToIdentityMap.at(variable);
        }
    }
    return globalVariablesInActionDd;
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::combineCommandsToActionMarkovChain(
    GenerationInformation& generationInfo, std::vector<ActionDecisionDiagram>& commandDds) {
    storm::dd::Bdd<Type> allGuards = generationInfo.manager->getBddZero();
    storm::dd::Add<Type, ValueType> allCommands = generationInfo.manager->template getAddZero<ValueType>();
    storm::dd::Bdd<Type> temporary;

    // Make all command DDs assign to the same global variables.
    std::set<storm::expressions::Variable> assignedGlobalVariables = equalizeAssignedGlobalVariables(generationInfo, commandDds);

    // Then combine the commands to the full action DD and multiply missing identities along the way.
    for (auto& commandDd : commandDds) {
        // Check for overlapping guards.
        temporary = commandDd.guardDd && allGuards;

        // Issue a warning if there are overlapping guards in a non-CTMC model.
        STORM_LOG_WARN_COND(temporary.isZero() || generationInfo.program.getModelType() == storm::prism::Program::ModelType::CTMC,
                            "Guard of a command overlaps with previous guards.");

        allGuards |= commandDd.guardDd;
        allCommands += commandDd.transitionsDd;
    }

    return ActionDecisionDiagram(allGuards, allCommands, assignedGlobalVariables);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> DdPrismModelBuilder<Type, ValueType>::encodeChoice(GenerationInformation& generationInfo,
                                                                                   uint_fast64_t nondeterminismVariableOffset,
                                                                                   uint_fast64_t numberOfBinaryVariables, int_fast64_t value) {
    storm::dd::Add<Type, ValueType> result = generationInfo.manager->template getAddZero<ValueType>();

    STORM_LOG_TRACE("Encoding " << value << " with " << numberOfBinaryVariables << " binary variable(s) starting from offset " << nondeterminismVariableOffset
                                << ".");

    std::map<storm::expressions::Variable, int_fast64_t> metaVariableNameToValueMap;
    for (uint_fast64_t i = 0; i < numberOfBinaryVariables; ++i) {
        if (value & (1ull << (numberOfBinaryVariables - i - 1))) {
            metaVariableNameToValueMap.emplace(generationInfo.nondeterminismMetaVariables[nondeterminismVariableOffset + i], 1);
        } else {
            metaVariableNameToValueMap.emplace(generationInfo.nondeterminismMetaVariables[nondeterminismVariableOffset + i], 0);
        }
    }

    result.setValue(metaVariableNameToValueMap, storm::utility::one<ValueType>());
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::combineCommandsToActionMDP(
    GenerationInformation& generationInfo, std::vector<ActionDecisionDiagram>& commandDds, uint_fast64_t nondeterminismVariableOffset) {
    storm::dd::Bdd<Type> allGuards = generationInfo.manager->getBddZero();
    storm::dd::Add<Type, ValueType> allCommands = generationInfo.manager->template getAddZero<ValueType>();

    // Make all command DDs assign to the same global variables.
    std::set<storm::expressions::Variable> assignedGlobalVariables = equalizeAssignedGlobalVariables(generationInfo, commandDds);

    // Sum all guards, so we can read off the maximal number of nondeterministic choices in any given state.
    storm::dd::Add<Type, uint_fast64_t> sumOfGuards = generationInfo.manager->template getAddZero<uint_fast64_t>();
    for (auto const& commandDd : commandDds) {
        sumOfGuards += commandDd.guardDd.template toAdd<uint_fast64_t>();
        allGuards |= commandDd.guardDd;
    }
    uint_fast64_t maxChoices = sumOfGuards.getMax();

    STORM_LOG_TRACE("Found " << maxChoices << " local choices.");

    // Depending on the maximal number of nondeterminstic choices, we need to use some variables to encode the nondeterminism.
    if (maxChoices == 0) {
        return ActionDecisionDiagram(*generationInfo.manager);
    } else if (maxChoices == 1) {
        // Sum up all commands.
        for (auto const& commandDd : commandDds) {
            allCommands += commandDd.transitionsDd;
        }
        return ActionDecisionDiagram(allGuards, allCommands, assignedGlobalVariables);
    } else {
        // Calculate number of required variables to encode the nondeterminism.
        uint_fast64_t numberOfBinaryVariables = static_cast<uint_fast64_t>(std::ceil(std::log2(maxChoices)));

        storm::dd::Bdd<Type> equalsNumberOfChoicesDd;
        std::vector<storm::dd::Add<Type, ValueType>> choiceDds(maxChoices, generationInfo.manager->template getAddZero<ValueType>());
        std::vector<storm::dd::Bdd<Type>> remainingDds(maxChoices, generationInfo.manager->getBddZero());

        for (uint_fast64_t currentChoices = 1; currentChoices <= maxChoices; ++currentChoices) {
            // Determine the set of states with exactly currentChoices choices.
            equalsNumberOfChoicesDd = sumOfGuards.equals(generationInfo.manager->getConstant(currentChoices));

            // If there is no such state, continue with the next possible number of choices.
            if (equalsNumberOfChoicesDd.isZero()) {
                continue;
            }

            // Reset the previously used intermediate storage.
            for (uint_fast64_t j = 0; j < currentChoices; ++j) {
                choiceDds[j] = generationInfo.manager->template getAddZero<ValueType>();
                remainingDds[j] = equalsNumberOfChoicesDd;
            }

            for (std::size_t j = 0; j < commandDds.size(); ++j) {
                // Check if command guard overlaps with equalsNumberOfChoicesDd. That is, there are states with exactly currentChoices
                // choices such that one outgoing choice is given by the j-th command.
                storm::dd::Bdd<Type> guardChoicesIntersection = commandDds[j].guardDd && equalsNumberOfChoicesDd;

                // If there is no such state, continue with the next command.
                if (guardChoicesIntersection.isZero()) {
                    continue;
                }

                // Split the nondeterministic choices.
                for (uint_fast64_t k = 0; k < currentChoices; ++k) {
                    // Calculate the overlapping part of command guard and the remaining DD.
                    storm::dd::Bdd<Type> remainingGuardChoicesIntersection = guardChoicesIntersection && remainingDds[k];

                    // Check if we can add some overlapping parts to the current index.
                    if (!remainingGuardChoicesIntersection.isZero()) {
                        // Remove overlapping parts from the remaining DD.
                        remainingDds[k] = remainingDds[k] && !remainingGuardChoicesIntersection;

                        // Combine the overlapping part of the guard with command updates and add it to the resulting DD.
                        choiceDds[k] += remainingGuardChoicesIntersection.template toAdd<ValueType>() * commandDds[j].transitionsDd;
                    }

                    // Remove overlapping parts from the command guard DD
                    guardChoicesIntersection = guardChoicesIntersection && !remainingGuardChoicesIntersection;

                    // If the guard DD has become equivalent to false, we can stop here.
                    if (guardChoicesIntersection.isZero()) {
                        break;
                    }
                }
            }

            // Add the meta variables that encode the nondeterminisim to the different choices.
            for (uint_fast64_t j = 0; j < currentChoices; ++j) {
                allCommands += encodeChoice(generationInfo, nondeterminismVariableOffset, numberOfBinaryVariables, j) * choiceDds[j];
            }

            // Delete currentChoices out of overlapping DD
            sumOfGuards = sumOfGuards * (!equalsNumberOfChoicesDd).template toAdd<uint_fast64_t>();
        }

        return ActionDecisionDiagram(allGuards, allCommands, assignedGlobalVariables, nondeterminismVariableOffset + numberOfBinaryVariables);
    }
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::combineSynchronizingActions(
    ActionDecisionDiagram const& action1, ActionDecisionDiagram const& action2) {
    std::set<storm::expressions::Variable> assignedGlobalVariables;
    std::set_union(action1.assignedGlobalVariables.begin(), action1.assignedGlobalVariables.end(), action2.assignedGlobalVariables.begin(),
                   action2.assignedGlobalVariables.end(), std::inserter(assignedGlobalVariables, assignedGlobalVariables.begin()));
    return ActionDecisionDiagram(action1.guardDd && action2.guardDd, action1.transitionsDd * action2.transitionsDd, assignedGlobalVariables,
                                 std::max(action1.numberOfUsedNondeterminismVariables, action2.numberOfUsedNondeterminismVariables));
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(
    GenerationInformation const& generationInfo, ActionDecisionDiagram& action1, ActionDecisionDiagram& action2,
    storm::dd::Add<Type, ValueType> const& identityDd1, storm::dd::Add<Type, ValueType> const& identityDd2) {
    // First extend the action DDs by the other identities.
    STORM_LOG_TRACE("Multiplying identities to combine unsynchronized actions.");
    action1.transitionsDd = action1.transitionsDd * identityDd2;
    action2.transitionsDd = action2.transitionsDd * identityDd1;

    // Then combine the extended action DDs.
    return combineUnsynchronizedActions(generationInfo, action1, action2);
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ActionDecisionDiagram DdPrismModelBuilder<Type, ValueType>::combineUnsynchronizedActions(
    GenerationInformation const& generationInfo, ActionDecisionDiagram& action1, ActionDecisionDiagram& action2) {
    STORM_LOG_TRACE("Combining unsynchronized actions.");

    // Make both action DDs write to the same global variables.
    std::set<storm::expressions::Variable> assignedGlobalVariables = equalizeAssignedGlobalVariables(generationInfo, action1, action2);

    if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC ||
        generationInfo.program.getModelType() == storm::prism::Program::ModelType::CTMC) {
        return ActionDecisionDiagram(action1.guardDd || action2.guardDd, action1.transitionsDd + action2.transitionsDd, assignedGlobalVariables, 0);
    } else if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
        if (action1.transitionsDd.isZero()) {
            return ActionDecisionDiagram(action2.guardDd, action2.transitionsDd, assignedGlobalVariables, action2.numberOfUsedNondeterminismVariables);
        } else if (action2.transitionsDd.isZero()) {
            return ActionDecisionDiagram(action1.guardDd, action1.transitionsDd, assignedGlobalVariables, action1.numberOfUsedNondeterminismVariables);
        }

        // Bring both choices to the same number of variables that encode the nondeterminism.
        uint_fast64_t numberOfUsedNondeterminismVariables = std::max(action1.numberOfUsedNondeterminismVariables, action2.numberOfUsedNondeterminismVariables);
        if (action1.numberOfUsedNondeterminismVariables > action2.numberOfUsedNondeterminismVariables) {
            storm::dd::Add<Type, ValueType> nondeterminismEncoding = generationInfo.manager->template getAddOne<ValueType>();

            for (uint_fast64_t i = action2.numberOfUsedNondeterminismVariables; i < action1.numberOfUsedNondeterminismVariables; ++i) {
                nondeterminismEncoding *= generationInfo.manager->getEncoding(generationInfo.nondeterminismMetaVariables[i], 0).template toAdd<ValueType>();
            }
            action2.transitionsDd *= nondeterminismEncoding;
        } else if (action2.numberOfUsedNondeterminismVariables > action1.numberOfUsedNondeterminismVariables) {
            storm::dd::Add<Type, ValueType> nondeterminismEncoding = generationInfo.manager->template getAddOne<ValueType>();

            for (uint_fast64_t i = action1.numberOfUsedNondeterminismVariables; i < action2.numberOfUsedNondeterminismVariables; ++i) {
                nondeterminismEncoding *= generationInfo.manager->getEncoding(generationInfo.nondeterminismMetaVariables[i], 0).template toAdd<ValueType>();
            }
            action1.transitionsDd *= nondeterminismEncoding;
        }

        // Add a new variable that resolves the nondeterminism between the two choices.
        storm::dd::Add<Type, ValueType> combinedTransitions =
            generationInfo.manager->getEncoding(generationInfo.nondeterminismMetaVariables[numberOfUsedNondeterminismVariables], 1)
                .ite(action2.transitionsDd, action1.transitionsDd);

        return ActionDecisionDiagram(action1.guardDd || action2.guardDd, combinedTransitions, assignedGlobalVariables, numberOfUsedNondeterminismVariables + 1);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidStateException, "Illegal model type.");
    }
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::ModuleDecisionDiagram DdPrismModelBuilder<Type, ValueType>::createModuleDecisionDiagram(
    GenerationInformation& generationInfo, storm::prism::Module const& module, std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap) {
    // Start by creating the action DD for the independent action.
    ActionDecisionDiagram independentActionDd = createActionDecisionDiagram(generationInfo, module, 0, 0);
    uint_fast64_t numberOfUsedNondeterminismVariables = independentActionDd.numberOfUsedNondeterminismVariables;

    // Create module DD for all synchronizing actions of the module.
    std::map<uint_fast64_t, ActionDecisionDiagram> actionIndexToDdMap;
    for (auto const& actionIndex : module.getSynchronizingActionIndices()) {
        STORM_LOG_TRACE("Creating DD for action '" << actionIndex << "'.");
        ActionDecisionDiagram tmp = createActionDecisionDiagram(generationInfo, module, actionIndex, synchronizingActionToOffsetMap.at(actionIndex));
        numberOfUsedNondeterminismVariables = std::max(numberOfUsedNondeterminismVariables, tmp.numberOfUsedNondeterminismVariables);
        actionIndexToDdMap.emplace(actionIndex, tmp);
    }

    return ModuleDecisionDiagram(independentActionDd, actionIndexToDdMap, generationInfo.moduleToIdentityMap.at(module.getName()),
                                 numberOfUsedNondeterminismVariables);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> DdPrismModelBuilder<Type, ValueType>::getSynchronizationDecisionDiagram(GenerationInformation& generationInfo,
                                                                                                        uint_fast64_t actionIndex) {
    storm::dd::Add<Type, ValueType> synchronization = generationInfo.manager->template getAddOne<ValueType>();
    if (actionIndex != 0) {
        for (uint_fast64_t i = 0; i < generationInfo.synchronizationMetaVariables.size(); ++i) {
            if ((actionIndex - 1) == i) {
                synchronization *= generationInfo.manager->getEncoding(generationInfo.synchronizationMetaVariables[i], 1).template toAdd<ValueType>();
            } else {
                synchronization *= generationInfo.manager->getEncoding(generationInfo.synchronizationMetaVariables[i], 0).template toAdd<ValueType>();
            }
        }
    } else {
        for (uint_fast64_t i = 0; i < generationInfo.synchronizationMetaVariables.size(); ++i) {
            synchronization *= generationInfo.manager->getEncoding(generationInfo.synchronizationMetaVariables[i], 0).template toAdd<ValueType>();
        }
    }
    return synchronization;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> DdPrismModelBuilder<Type, ValueType>::createSystemFromModule(GenerationInformation& generationInfo,
                                                                                             ModuleDecisionDiagram& module) {
    storm::dd::Add<Type, ValueType> result;

    // Make sure all actions contain all necessary meta variables.
    module.independentAction.ensureContainsVariables(generationInfo.rowMetaVariables, generationInfo.columnMetaVariables);
    for (auto& synchronizingAction : module.synchronizingActionToDecisionDiagramMap) {
        synchronizingAction.second.ensureContainsVariables(generationInfo.rowMetaVariables, generationInfo.columnMetaVariables);
    }

    // If the model is an MDP, we need to encode the nondeterminism using additional variables.
    if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
        result = generationInfo.manager->template getAddZero<ValueType>();

        // First, determine the highest number of nondeterminism variables that is used in any action and make
        // all actions use the same amout of nondeterminism variables.
        uint_fast64_t numberOfUsedNondeterminismVariables = module.numberOfUsedNondeterminismVariables;

        // Compute missing global variable identities in independent action.
        std::set<storm::expressions::Variable> missingIdentities;
        std::set_difference(generationInfo.allGlobalVariables.begin(), generationInfo.allGlobalVariables.end(),
                            module.independentAction.assignedGlobalVariables.begin(), module.independentAction.assignedGlobalVariables.end(),
                            std::inserter(missingIdentities, missingIdentities.begin()));
        storm::dd::Add<Type, ValueType> identityEncoding = generationInfo.manager->template getAddOne<ValueType>();
        for (auto const& variable : missingIdentities) {
            STORM_LOG_TRACE("Multiplying identity of global variable " << variable.getName() << " to independent action.");
            identityEncoding *= generationInfo.variableToIdentityMap.at(variable);
        }

        // Add variables to independent action DD.
        storm::dd::Add<Type, ValueType> nondeterminismEncoding = generationInfo.manager->template getAddOne<ValueType>();
        for (uint_fast64_t i = module.independentAction.numberOfUsedNondeterminismVariables; i < numberOfUsedNondeterminismVariables; ++i) {
            nondeterminismEncoding *= generationInfo.manager->getEncoding(generationInfo.nondeterminismMetaVariables[i], 0).template toAdd<ValueType>();
        }

        result = identityEncoding * module.independentAction.transitionsDd * nondeterminismEncoding;

        // Add variables to synchronized action DDs.
        std::map<uint_fast64_t, storm::dd::Add<Type, ValueType>> synchronizingActionToDdMap;
        for (auto const& synchronizingAction : module.synchronizingActionToDecisionDiagramMap) {
            // Compute missing global variable identities in synchronizing actions.
            missingIdentities = std::set<storm::expressions::Variable>();
            std::set_difference(generationInfo.allGlobalVariables.begin(), generationInfo.allGlobalVariables.end(),
                                synchronizingAction.second.assignedGlobalVariables.begin(), synchronizingAction.second.assignedGlobalVariables.end(),
                                std::inserter(missingIdentities, missingIdentities.begin()));
            identityEncoding = generationInfo.manager->template getAddOne<ValueType>();
            for (auto const& variable : missingIdentities) {
                STORM_LOG_TRACE("Multiplying identity of global variable " << variable.getName() << " to synchronizing action '" << synchronizingAction.first
                                                                           << "'.");
                identityEncoding *= generationInfo.variableToIdentityMap.at(variable);
            }

            nondeterminismEncoding = generationInfo.manager->template getAddOne<ValueType>();
            for (uint_fast64_t i = synchronizingAction.second.numberOfUsedNondeterminismVariables; i < numberOfUsedNondeterminismVariables; ++i) {
                nondeterminismEncoding *= generationInfo.manager->getEncoding(generationInfo.nondeterminismMetaVariables[i], 0).template toAdd<ValueType>();
            }
            synchronizingActionToDdMap.emplace(synchronizingAction.first, identityEncoding * synchronizingAction.second.transitionsDd * nondeterminismEncoding);
        }

        // Add variables for synchronization.
        result *= getSynchronizationDecisionDiagram(generationInfo);

        for (auto& synchronizingAction : synchronizingActionToDdMap) {
            synchronizingAction.second *= getSynchronizationDecisionDiagram(generationInfo, synchronizingAction.first);
        }

        // Now, we can simply add all synchronizing actions to the result.
        for (auto const& synchronizingAction : synchronizingActionToDdMap) {
            result += synchronizingAction.second;
        }
    } else if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC ||
               generationInfo.program.getModelType() == storm::prism::Program::ModelType::CTMC) {
        // Simply add all actions, but make sure to include the missing global variable identities.

        // Compute missing global variable identities in independent action.
        std::set<storm::expressions::Variable> missingIdentities;
        std::set_difference(generationInfo.allGlobalVariables.begin(), generationInfo.allGlobalVariables.end(),
                            module.independentAction.assignedGlobalVariables.begin(), module.independentAction.assignedGlobalVariables.end(),
                            std::inserter(missingIdentities, missingIdentities.begin()));
        storm::dd::Add<Type, ValueType> identityEncoding = generationInfo.manager->template getAddOne<ValueType>();
        for (auto const& variable : missingIdentities) {
            STORM_LOG_TRACE("Multiplying identity of global variable " << variable.getName() << " to independent action.");
            identityEncoding *= generationInfo.variableToIdentityMap.at(variable);
        }

        result = identityEncoding * module.independentAction.transitionsDd;
        for (auto const& synchronizingAction : module.synchronizingActionToDecisionDiagramMap) {
            // Compute missing global variable identities in synchronizing actions.
            missingIdentities = std::set<storm::expressions::Variable>();
            std::set_difference(generationInfo.allGlobalVariables.begin(), generationInfo.allGlobalVariables.end(),
                                synchronizingAction.second.assignedGlobalVariables.begin(), synchronizingAction.second.assignedGlobalVariables.end(),
                                std::inserter(missingIdentities, missingIdentities.begin()));
            identityEncoding = generationInfo.manager->template getAddOne<ValueType>();
            for (auto const& variable : missingIdentities) {
                STORM_LOG_TRACE("Multiplying identity of global variable " << variable.getName() << " to synchronizing action '" << synchronizingAction.first
                                                                           << "'.");
                identityEncoding *= generationInfo.variableToIdentityMap.at(variable);
            }

            result += identityEncoding * synchronizingAction.second.transitionsDd;
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Illegal model type.");
    }
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
typename DdPrismModelBuilder<Type, ValueType>::SystemResult DdPrismModelBuilder<Type, ValueType>::createSystemDecisionDiagram(
    GenerationInformation& generationInfo) {
    ModuleComposer<Type, ValueType> composer(generationInfo);
    ModuleDecisionDiagram system =
        composer.compose(generationInfo.program.specifiesSystemComposition() ? generationInfo.program.getSystemCompositionConstruct().getSystemComposition()
                                                                             : *generationInfo.program.getDefaultSystemComposition());

    storm::dd::Add<Type, ValueType> result = createSystemFromModule(generationInfo, system);

    // Create an auxiliary DD that is used later during the construction of reward models.
    boost::optional<storm::dd::Add<Type, ValueType>> stateActionDd;

    // For DTMCs, we normalize each row to 1 (to account for non-determinism).
    if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC) {
        stateActionDd = result.sumAbstract(generationInfo.columnMetaVariables);
        result = result / stateActionDd.get();
    } else if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
        // For MDPs, we need to throw away the nondeterminism variables from the generation information that
        // were never used.
        for (uint_fast64_t index = system.numberOfUsedNondeterminismVariables; index < generationInfo.nondeterminismMetaVariables.size(); ++index) {
            generationInfo.allNondeterminismVariables.erase(generationInfo.nondeterminismMetaVariables[index]);
        }
        generationInfo.nondeterminismMetaVariables.resize(system.numberOfUsedNondeterminismVariables);
    }

    return SystemResult(result, system, stateActionDd);
}

template<storm::dd::DdType Type, typename ValueType>
std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>>
DdPrismModelBuilder<Type, ValueType>::createRewardModelDecisionDiagrams(
    std::vector<std::reference_wrapper<storm::prism::RewardModel const>> const& selectedRewardModels, SystemResult& system,
    GenerationInformation& generationInfo, ModuleDecisionDiagram const& globalModule, storm::dd::Add<Type, ValueType> const& reachableStatesAdd,
    storm::dd::Add<Type, ValueType> const& transitionMatrix) {
    std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> rewardModels;
    for (auto const& rewardModel : selectedRewardModels) {
        rewardModels.emplace(rewardModel.get().getName(), createRewardModelDecisionDiagrams(generationInfo, rewardModel.get(), globalModule, reachableStatesAdd,
                                                                                            transitionMatrix, system.stateActionDd));
    }
    return rewardModels;
}

template<storm::dd::DdType Type, typename ValueType>
void checkRewards(storm::dd::Add<Type, ValueType> const& rewards, std::string const& rewardType) {
    STORM_LOG_WARN_COND(rewards.getMin() >= 0, "The reward model assigns negative " << rewardType << " to some states.");
    STORM_LOG_WARN_COND(!rewards.isZero(), "The reward model declares " << rewardType << " but does not assign any non-zero values.");
}

template<storm::dd::DdType Type>
void checkRewards(storm::dd::Add<Type, storm::RationalFunction> const& rewards, std::string const& rewardType) {
    STORM_LOG_WARN_COND(!rewards.isZero(), "The reward model declares " << rewardType << " but does not assign any non-zero values.");
}

template<storm::dd::DdType Type, typename ValueType>
storm::models::symbolic::StandardRewardModel<Type, ValueType> DdPrismModelBuilder<Type, ValueType>::createRewardModelDecisionDiagrams(
    GenerationInformation& generationInfo, storm::prism::RewardModel const& rewardModel, ModuleDecisionDiagram const& globalModule,
    storm::dd::Add<Type, ValueType> const& reachableStatesAdd, storm::dd::Add<Type, ValueType> const& transitionMatrix,
    boost::optional<storm::dd::Add<Type, ValueType>>& stateActionDd) {
    // Start by creating the state reward vector.
    boost::optional<storm::dd::Add<Type, ValueType>> stateRewards;
    if (rewardModel.hasStateRewards()) {
        stateRewards = generationInfo.manager->template getAddZero<ValueType>();

        for (auto const& stateReward : rewardModel.getStateRewards()) {
            storm::dd::Add<Type, ValueType> states = generationInfo.rowExpressionAdapter->translateExpression(stateReward.getStatePredicateExpression());
            storm::dd::Add<Type, ValueType> rewards = generationInfo.rowExpressionAdapter->translateExpression(stateReward.getRewardValueExpression());

            // Restrict the rewards to those states that satisfy the condition.
            rewards = reachableStatesAdd * states * rewards;

            // Add the rewards to the global state reward vector.
            stateRewards.get() += rewards;
        }
        // Perform some sanity checks.
        checkRewards(stateRewards.get(), "state rewards");
    }

    // Next, build the state-action reward vector.
    boost::optional<storm::dd::Add<Type, ValueType>> stateActionRewards;
    if (rewardModel.hasStateActionRewards()) {
        stateActionRewards = generationInfo.manager->template getAddZero<ValueType>();

        for (auto const& stateActionReward : rewardModel.getStateActionRewards()) {
            storm::dd::Add<Type, ValueType> states = generationInfo.rowExpressionAdapter->translateExpression(stateActionReward.getStatePredicateExpression());
            storm::dd::Add<Type, ValueType> rewards = generationInfo.rowExpressionAdapter->translateExpression(stateActionReward.getRewardValueExpression());
            storm::dd::Add<Type, ValueType> synchronization = generationInfo.manager->template getAddOne<ValueType>();

            if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
                synchronization = getSynchronizationDecisionDiagram(generationInfo, stateActionReward.getActionIndex());
            }
            ActionDecisionDiagram const& actionDd = stateActionReward.isLabeled()
                                                        ? globalModule.synchronizingActionToDecisionDiagramMap.at(stateActionReward.getActionIndex())
                                                        : globalModule.independentAction;
            states *= actionDd.guardDd.template toAdd<ValueType>() * reachableStatesAdd;
            storm::dd::Add<Type, ValueType> stateActionRewardDd = synchronization * states * rewards;

            // If we are building the state-action rewards for an MDP, we need to make sure that the reward is
            // only given on legal nondeterminism encodings, which is why we multiply with the state-action DD.
            if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
                if (!stateActionDd) {
                    stateActionDd = transitionMatrix.notZero().existsAbstract(generationInfo.columnMetaVariables).template toAdd<ValueType>();
                }
                stateActionRewardDd *= stateActionDd.get();
            } else if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC ||
                       generationInfo.program.getModelType() == storm::prism::Program::ModelType::CTMC) {
                // For DTMCs and CTMC, we need to multiply the entries with the multiplicity/exit rate of the corresponding action.
                stateActionRewardDd *= actionDd.transitionsDd.sumAbstract(generationInfo.columnMetaVariables);
            }

            // Add the rewards to the global transition reward matrix.
            stateActionRewards.get() += stateActionRewardDd;
        }

        // Scale state-action rewards for DTMCs and CTMCs.
        if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC ||
            generationInfo.program.getModelType() == storm::prism::Program::ModelType::CTMC) {
            if (!stateActionDd) {
                stateActionDd = transitionMatrix.sumAbstract(generationInfo.columnMetaVariables);
            }

            stateActionRewards.get() /= stateActionDd.get();
        }

        // Perform some sanity checks.
        checkRewards(stateActionRewards.get(), "action rewards");
    }

    // Then build the transition reward matrix.
    boost::optional<storm::dd::Add<Type, ValueType>> transitionRewards;
    if (rewardModel.hasTransitionRewards()) {
        transitionRewards = generationInfo.manager->template getAddZero<ValueType>();

        for (auto const& transitionReward : rewardModel.getTransitionRewards()) {
            storm::dd::Add<Type, ValueType> sourceStates =
                generationInfo.rowExpressionAdapter->translateExpression(transitionReward.getSourceStatePredicateExpression());
            storm::dd::Add<Type, ValueType> targetStates =
                generationInfo.rowExpressionAdapter->translateExpression(transitionReward.getTargetStatePredicateExpression());
            storm::dd::Add<Type, ValueType> rewards = generationInfo.rowExpressionAdapter->translateExpression(transitionReward.getRewardValueExpression());

            storm::dd::Add<Type, ValueType> synchronization = generationInfo.manager->template getAddOne<ValueType>();

            storm::dd::Add<Type, ValueType> transitions;
            if (transitionReward.isLabeled()) {
                if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
                    synchronization = getSynchronizationDecisionDiagram(generationInfo, transitionReward.getActionIndex());
                }
                transitions = globalModule.synchronizingActionToDecisionDiagramMap.at(transitionReward.getActionIndex()).transitionsDd;
            } else {
                if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::MDP) {
                    synchronization = getSynchronizationDecisionDiagram(generationInfo);
                }
                transitions = globalModule.independentAction.transitionsDd;
            }

            storm::dd::Add<Type, ValueType> transitionRewardDd = synchronization * sourceStates * targetStates * rewards;
            if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC) {
                // For DTMCs we need to keep the weighting for the scaling that follows.
                transitionRewardDd = transitions * transitionRewardDd;
            } else {
                // For all other model types, we do not scale the rewards.
                transitionRewardDd = transitions.notZero().template toAdd<ValueType>() * transitionRewardDd;
            }

            // Add the rewards to the global transition reward matrix.
            transitionRewards.get() += transitionRewardDd;
        }

        // Perform some sanity checks.
        checkRewards(transitionRewards.get(), "transition rewards");

        // Scale transition rewards for DTMCs.
        if (generationInfo.program.getModelType() == storm::prism::Program::ModelType::DTMC) {
            transitionRewards.get() /= stateActionDd.get();
        }
    }

    return storm::models::symbolic::StandardRewardModel<Type, ValueType>(stateRewards, stateActionRewards, transitionRewards);
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> DdPrismModelBuilder<Type, ValueType>::buildInternal(
    storm::prism::Program const& program, Options const& options, std::shared_ptr<storm::dd::DdManager<Type>> const& manager) {
    // Start by initializing the structure used for storing all information needed during the model generation.
    // In particular, this creates the meta variables used to encode the model.
    GenerationInformation generationInfo(program, manager);

    SystemResult system = createSystemDecisionDiagram(generationInfo);
    storm::dd::Add<Type, ValueType> transitionMatrix = system.allTransitionsDd;

    ModuleDecisionDiagram const& globalModule = system.globalModule;

    // If we were asked to treat some states as terminal states, we cut away their transitions now.
    storm::dd::Bdd<Type> terminalStatesBdd = generationInfo.manager->getBddZero();
    if (!options.terminalStates.empty()) {
        storm::expressions::Expression terminalExpression = options.terminalStates.asExpression([&program](std::string const& labelName) {
            if (program.hasLabel(labelName)) {
                return program.getLabelExpression(labelName);
            } else {
                STORM_LOG_THROW(labelName == "init" || labelName == "deadlock", storm::exceptions::InvalidArgumentException,
                                "Terminal states refer to illegal label '" << labelName << "'.");
                // If the label name is "init" we can abort 'exploration' directly at the initial state. If it is deadlock, we do not have to abort.
                return program.getManager().boolean(labelName == "init");
            }
        });
        terminalExpression = terminalExpression.substitute(program.getConstantsSubstitution());
        terminalStatesBdd = generationInfo.rowExpressionAdapter->translateExpression(terminalExpression).toBdd();
        transitionMatrix *= (!terminalStatesBdd).template toAdd<ValueType>();
    }

    // Cut the transitions and rewards to the reachable fragment of the state space.
    storm::dd::Bdd<Type> initialStates = createInitialStatesDecisionDiagram(generationInfo);

    storm::dd::Bdd<Type> transitionMatrixBdd = transitionMatrix.notZero();
    if (program.getModelType() == storm::prism::Program::ModelType::MDP) {
        transitionMatrixBdd = transitionMatrixBdd.existsAbstract(generationInfo.allNondeterminismVariables);
    }

    storm::dd::Bdd<Type> reachableStates = storm::utility::dd::computeReachableStates<Type>(initialStates, transitionMatrixBdd, generationInfo.rowMetaVariables,
                                                                                            generationInfo.columnMetaVariables)
                                               .first;
    storm::dd::Add<Type, ValueType> reachableStatesAdd = reachableStates.template toAdd<ValueType>();
    transitionMatrix *= reachableStatesAdd;
    if (system.stateActionDd) {
        system.stateActionDd.get() *= reachableStatesAdd;
    }

    // Detect deadlocks and 1) fix them if requested 2) throw an error otherwise.
    storm::dd::Bdd<Type> statesWithTransition = transitionMatrixBdd.existsAbstract(generationInfo.columnMetaVariables);
    storm::dd::Bdd<Type> deadlockStates = reachableStates && !statesWithTransition;

    // If there are deadlocks, either fix them or raise an error.
    if (!deadlockStates.isZero()) {
        // If we need to fix deadlocks, we do so now.
        if (options.fixDeadlocks) {
            STORM_LOG_INFO("Fixing deadlocks in " << deadlockStates.getNonZeroCount() << " states. The first three of these states are: ");

            storm::dd::Add<Type, ValueType> deadlockStatesAdd = deadlockStates.template toAdd<ValueType>();
            uint_fast64_t count = 0;
            for (auto it = deadlockStatesAdd.begin(), ite = deadlockStatesAdd.end(); it != ite && count < 3; ++it, ++count) {
                STORM_LOG_INFO((*it).first.toPrettyString(generationInfo.rowMetaVariables) << '\n');
            }

            if (program.getModelType() == storm::prism::Program::ModelType::DTMC || program.getModelType() == storm::prism::Program::ModelType::CTMC) {
                storm::dd::Add<Type, ValueType> identity = globalModule.identity;

                // Make sure that global variables do not change along the introduced self-loops.
                for (auto const& var : generationInfo.allGlobalVariables) {
                    identity *= generationInfo.variableToIdentityMap.at(var);
                }

                // For DTMCs, we can simply add the identity of the global module for all deadlock states.
                transitionMatrix += deadlockStatesAdd * identity;
            } else if (program.getModelType() == storm::prism::Program::ModelType::MDP) {
                // For MDPs, however, we need to select an action associated with the self-loop, if we do not
                // want to attach a lot of self-loops to the deadlock states.
                storm::dd::Add<Type, ValueType> action = generationInfo.manager->template getAddOne<ValueType>();
                for (auto const& metaVariable : generationInfo.allNondeterminismVariables) {
                    action *= generationInfo.manager->template getIdentity<ValueType>(metaVariable);
                }
                // Make sure that global variables do not change along the introduced self-loops.
                for (auto const& var : generationInfo.allGlobalVariables) {
                    action *= generationInfo.variableToIdentityMap.at(var);
                }
                transitionMatrix += deadlockStatesAdd * globalModule.identity * action;
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                            "The model contains " << deadlockStates.getNonZeroCount()
                                                  << " deadlock states. Please unset the option to not fix deadlocks, if you want to fix them automatically.");
        }
    }

    // Reduce the deadlock states by the states that we did simply not explore.
    deadlockStates = deadlockStates && !terminalStatesBdd;

    // Now build the reward models.
    std::vector<std::reference_wrapper<storm::prism::RewardModel const>> selectedRewardModels;

    // First, we make sure that all selected reward models actually exist.
    for (auto const& rewardModelName : options.rewardModelsToBuild) {
        STORM_LOG_THROW(rewardModelName.empty() || program.hasRewardModel(rewardModelName), storm::exceptions::InvalidArgumentException,
                        "Model does not possess a reward model with the name '" << rewardModelName << "'.");
    }

    for (auto const& rewardModel : program.getRewardModels()) {
        if (options.buildAllRewardModels || options.rewardModelsToBuild.find(rewardModel.getName()) != options.rewardModelsToBuild.end()) {
            selectedRewardModels.push_back(rewardModel);
        }
    }
    // If no reward model was selected until now and a referenced reward model appears to be unique, we build
    // the only existing reward model (given that no explicit name was given for the referenced reward model).
    if (selectedRewardModels.empty() && program.getNumberOfRewardModels() == 1 && options.rewardModelsToBuild.size() == 1 &&
        *options.rewardModelsToBuild.begin() == "") {
        selectedRewardModels.push_back(program.getRewardModel(0));
    }

    std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> rewardModels =
        createRewardModelDecisionDiagrams(selectedRewardModels, system, generationInfo, globalModule, reachableStatesAdd, transitionMatrix);

    // Build the labels that can be accessed as a shortcut.
    std::map<std::string, storm::expressions::Expression> labelToExpressionMapping;
    for (auto const& label : program.getLabels()) {
        labelToExpressionMapping.emplace(label.getName(), label.getStatePredicateExpression());
    }

    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> result;
    if (program.getModelType() == storm::prism::Program::ModelType::DTMC) {
        result = std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>>(new storm::models::symbolic::Dtmc<Type, ValueType>(
            generationInfo.manager, reachableStates, initialStates, deadlockStates, transitionMatrix, generationInfo.rowMetaVariables,
            generationInfo.rowExpressionAdapter, generationInfo.columnMetaVariables, generationInfo.rowColumnMetaVariablePairs, labelToExpressionMapping,
            rewardModels));
    } else if (program.getModelType() == storm::prism::Program::ModelType::CTMC) {
        result = std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>>(new storm::models::symbolic::Ctmc<Type, ValueType>(
            generationInfo.manager, reachableStates, initialStates, deadlockStates, transitionMatrix, system.stateActionDd, generationInfo.rowMetaVariables,
            generationInfo.rowExpressionAdapter, generationInfo.columnMetaVariables, generationInfo.rowColumnMetaVariablePairs, labelToExpressionMapping,
            rewardModels));
    } else if (program.getModelType() == storm::prism::Program::ModelType::MDP) {
        result = std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>>(new storm::models::symbolic::Mdp<Type, ValueType>(
            generationInfo.manager, reachableStates, initialStates, deadlockStates, transitionMatrix, generationInfo.rowMetaVariables,
            generationInfo.rowExpressionAdapter, generationInfo.columnMetaVariables, generationInfo.rowColumnMetaVariablePairs,
            generationInfo.allNondeterminismVariables, labelToExpressionMapping, rewardModels));
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Invalid model type.");
    }

    if (std::is_same<ValueType, storm::RationalFunction>::value) {
        result->addParameters(generationInfo.parameters);
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> DdPrismModelBuilder<Type, ValueType>::build(storm::Environment const& env,
                                                                                                             storm::prism::Program const& program,
                                                                                                             Options const& options) {
    if (!std::is_same<ValueType, storm::RationalFunction>::value && program.hasUndefinedConstants()) {
        std::vector<std::reference_wrapper<storm::prism::Constant const>> undefinedConstants = program.getUndefinedConstants();
        std::stringstream stream;
        bool printComma = false;
        for (auto const& constant : undefinedConstants) {
            if (printComma) {
                stream << ", ";
            } else {
                printComma = true;
            }
            stream << constant.get().getName() << " (" << constant.get().getType() << ")";
        }
        stream << ".";
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Program still contains these undefined constants: " + stream.str() + ".");
    }
    STORM_LOG_THROW(!program.hasUnboundedVariables(), storm::exceptions::InvalidArgumentException,
                    "Program contains unbounded variables which is not supported by the DD engine.");
    STORM_LOG_THROW(!program.hasIntervalUpdates(), storm::exceptions::InvalidArgumentException,
                    "Program contains interval updates which are not supported by the DD engnie.");

    STORM_LOG_TRACE("Building representation of program:\n" << program << '\n');

    auto manager = std::make_shared<storm::dd::DdManager<Type>>(env);
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> result;
    manager->execute([&program, &options, &manager, &result, this]() { result = this->buildInternal(program, options, manager); });
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> DdPrismModelBuilder<Type, ValueType>::createInitialStatesDecisionDiagram(GenerationInformation& generationInfo) {
    storm::dd::Bdd<Type> initialStates = generationInfo.rowExpressionAdapter->translateExpression(generationInfo.program.getInitialStatesExpression()).toBdd();

    for (auto const& metaVariable : generationInfo.rowMetaVariables) {
        initialStates &= generationInfo.manager->getRange(metaVariable);
    }

    return initialStates;
}

// Explicitly instantiate the symbolic model builder.
template class DdPrismModelBuilder<storm::dd::DdType::CUDD>;
template class DdPrismModelBuilder<storm::dd::DdType::Sylvan>;

template class DdPrismModelBuilder<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class DdPrismModelBuilder<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace builder
}  // namespace storm
