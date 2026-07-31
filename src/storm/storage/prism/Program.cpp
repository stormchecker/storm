#include "storm/storage/prism/Program.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <sstream>

#include "storm/exceptions/InternalException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/storage/prism/CompositionVisitor.h"
#include "storm/storage/prism/Compositions.h"
#include "storm/storage/prism/ToJaniConverter.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"
#include "storm/utility/vector.h"

namespace storm {
namespace prism {
class CompositionValidityChecker : public CompositionVisitor {
   public:
    CompositionValidityChecker(storm::prism::Program const& program) : program(program) {
        // Intentionally left empty.
    }

    void check(Composition const& composition) {
        composition.accept(*this, boost::any());
        STORM_LOG_THROW(appearingModules.size() == program.getNumberOfModules(), storm::exceptions::WrongFormatException,
                        "Not every module is used in the system composition.");
    }

    virtual boost::any visit(ModuleComposition const& composition, boost::any const&) override {
        bool isValid = program.hasModule(composition.getModuleName());
        STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                        "The module \"" << composition.getModuleName() << "\" referred to in the system composition does not exist.");
        isValid = appearingModules.find(composition.getModuleName()) == appearingModules.end();
        STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                        "The module \"" << composition.getModuleName() << "\" is referred to more than once in the system composition.");
        appearingModules.insert(composition.getModuleName());
        std::set<uint_fast64_t> synchronizingActionIndices = program.getModule(composition.getModuleName()).getSynchronizingActionIndices();
        return synchronizingActionIndices;
    }

    virtual boost::any visit(RenamingComposition const& composition, boost::any const& data) override {
        std::set<uint_fast64_t> subSynchronizingActionIndices = boost::any_cast<std::set<uint_fast64_t>>(composition.getSubcomposition().accept(*this, data));

        std::set<uint_fast64_t> newSynchronizingActionIndices = subSynchronizingActionIndices;
        for (auto const& namePair : composition.getActionRenaming()) {
            if (!program.hasAction(namePair.first)) {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "System composition refers to unknown action '" << namePair.first << "'.");
            } else if (!program.hasAction(namePair.second)) {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "System composition refers to unknown action '" << namePair.second << "'.");
            } else {
                uint_fast64_t fromIndex = program.getActionIndex(namePair.first);
                uint_fast64_t toIndex = program.getActionIndex(namePair.second);
                auto it = subSynchronizingActionIndices.find(fromIndex);
                STORM_LOG_THROW(
                    it != subSynchronizingActionIndices.end(), storm::exceptions::WrongFormatException,
                    "Cannot rename action '" << namePair.first << "', because module '" << composition.getSubcomposition() << " does not have this action.");
                newSynchronizingActionIndices.erase(newSynchronizingActionIndices.find(fromIndex));
                newSynchronizingActionIndices.insert(toIndex);
            }
        }

        return newSynchronizingActionIndices;
    }

    virtual boost::any visit(HidingComposition const& composition, boost::any const& data) override {
        std::set<uint_fast64_t> subSynchronizingActionIndices = boost::any_cast<std::set<uint_fast64_t>>(composition.getSubcomposition().accept(*this, data));

        for (auto const& action : composition.getActionsToHide()) {
            if (!program.hasAction(action)) {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "System composition refers to unknown action '" << action << "'.");
            } else {
                uint_fast64_t index = program.getActionIndex(action);
                auto it = subSynchronizingActionIndices.find(index);
                STORM_LOG_THROW(it != subSynchronizingActionIndices.end(), storm::exceptions::WrongFormatException,
                                "Cannot hide action '" << action << "', because module '" << composition.getSubcomposition() << " does not have this action.");
                subSynchronizingActionIndices.erase(it);
            }
        }

        return subSynchronizingActionIndices;
    }

    virtual boost::any visit(SynchronizingParallelComposition const& composition, boost::any const& data) override {
        std::set<uint_fast64_t> leftSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getLeftSubcomposition().accept(*this, data));
        std::set<uint_fast64_t> rightSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getRightSubcomposition().accept(*this, data));

        std::set<uint_fast64_t> synchronizingActionIndices;
        std::set_union(leftSynchronizingActionIndices.begin(), leftSynchronizingActionIndices.end(), rightSynchronizingActionIndices.begin(),
                       rightSynchronizingActionIndices.end(), std::inserter(synchronizingActionIndices, synchronizingActionIndices.begin()));

        return synchronizingActionIndices;
    }

    virtual boost::any visit(InterleavingParallelComposition const& composition, boost::any const& data) override {
        std::set<uint_fast64_t> leftSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getLeftSubcomposition().accept(*this, data));
        std::set<uint_fast64_t> rightSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getRightSubcomposition().accept(*this, data));

        std::set<uint_fast64_t> synchronizingActionIndices;
        std::set_union(leftSynchronizingActionIndices.begin(), leftSynchronizingActionIndices.end(), rightSynchronizingActionIndices.begin(),
                       rightSynchronizingActionIndices.end(), std::inserter(synchronizingActionIndices, synchronizingActionIndices.begin()));

        return synchronizingActionIndices;
    }

    virtual boost::any visit(RestrictedParallelComposition const& composition, boost::any const& data) override {
        std::set<uint_fast64_t> leftSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getLeftSubcomposition().accept(*this, data));
        std::set<uint_fast64_t> rightSynchronizingActionIndices =
            boost::any_cast<std::set<uint_fast64_t>>(composition.getRightSubcomposition().accept(*this, data));

        for (auto const& action : composition.getSynchronizingActions()) {
            if (!program.hasAction(action)) {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "System composition refers to unknown action '" << action << "'.");
            } else {
                uint_fast64_t index = program.getActionIndex(action);
                auto it = leftSynchronizingActionIndices.find(index);
                STORM_LOG_THROW(it != leftSynchronizingActionIndices.end(), storm::exceptions::WrongFormatException,
                                "Cannot synchronize on action '" << action << "', because module '" << composition.getLeftSubcomposition()
                                                                 << " does not have this action.");
                it = rightSynchronizingActionIndices.find(index);
                STORM_LOG_THROW(it != rightSynchronizingActionIndices.end(), storm::exceptions::WrongFormatException,
                                "Cannot synchronize on action '" << action << "', because module '" << composition.getRightSubcomposition()
                                                                 << " does not have this action.");
            }
        }

        std::set<uint_fast64_t> synchronizingActionIndices;
        std::set_union(leftSynchronizingActionIndices.begin(), leftSynchronizingActionIndices.end(), rightSynchronizingActionIndices.begin(),
                       rightSynchronizingActionIndices.end(), std::inserter(synchronizingActionIndices, synchronizingActionIndices.begin()));

        return synchronizingActionIndices;
    }

   private:
    storm::prism::Program const& program;
    std::set<std::string> appearingModules;
};

Program::Program(std::shared_ptr<storm::expressions::ExpressionManager> manager, ModelType modelType, std::vector<Constant> const& constants,
                 std::vector<BooleanVariable> const& globalBooleanVariables, std::vector<IntegerVariable> const& globalIntegerVariables,
                 std::vector<Formula> const& formulas, std::vector<Player> const& players, std::vector<Module> const& modules,
                 std::map<std::string, uint_fast64_t> const& actionToIndexMap, std::vector<RewardModel> const& rewardModels, std::vector<Label> const& labels,
                 std::vector<ObservationLabel> const& observationLabels, boost::optional<InitialConstruct> const& initialConstruct,
                 boost::optional<SystemCompositionConstruct> const& compositionConstruct, bool prismCompatibility, std::string const& filename,
                 uint_fast64_t lineNumber, bool finalModel)
    : LocatedInformation(filename, lineNumber),
      manager(manager),
      modelType(modelType),
      constants(constants),
      constantToIndexMap(),
      globalBooleanVariables(globalBooleanVariables),
      globalBooleanVariableToIndexMap(),
      globalIntegerVariables(globalIntegerVariables),
      globalIntegerVariableToIndexMap(),
      formulas(formulas),
      formulaToIndexMap(),
      players(players),
      modules(modules),
      moduleToIndexMap(),
      rewardModels(rewardModels),
      rewardModelToIndexMap(),
      systemCompositionConstruct(compositionConstruct),
      labels(labels),
      labelToIndexMap(),
      observationLabels(observationLabels),
      actionToIndexMap(actionToIndexMap),
      indexToActionMap(),
      actions(),
      synchronizingActionIndices(),
      actionIndicesToModuleIndexMap(),
      variableToModuleIndexMap(),
      possiblySynchronizingCommands(),
      prismCompatibility(prismCompatibility) {
    // Start by creating the necessary mappings from the given ones.
    this->createMappings();

    // Set the initial construct if given.
    if (initialConstruct) {
        this->initialConstruct = initialConstruct.get();
    } else {
        // Otherwise, we create the missing initial values.
        this->createMissingInitialValues();
        for (auto& modules : this->modules) {
            modules.createMissingInitialValues();
        }
    }

    uint64_t highestGlobalIndex = this->getHighestCommandIndex();
    possiblySynchronizingCommands = storage::BitVector(highestGlobalIndex + 1);
    std::set<uint64_t> possiblySynchronizingActionIndices;
    for (uint64_t syncAction : synchronizingActionIndices) {
        if (getModuleIndicesByActionIndex(syncAction).size() > 1) {
            possiblySynchronizingActionIndices.insert(syncAction);
        }
    }
    for (auto const& module : getModules()) {
        for (auto const& command : module.getCommands()) {
            if (command.isLabeled()) {
                if (possiblySynchronizingActionIndices.count(command.getActionIndex()) > 0) {
                    possiblySynchronizingCommands.set(command.getGlobalIndex());
                }
            }
        }
    }

    if (finalModel) {
        // If the model is supposed to be a CTMC, but contains probabilistic commands, we transform them to Markovian
        // commands and issue a warning.
        if (modelType == storm::prism::Program::ModelType::CTMC && prismCompatibility) {
            bool hasProbabilisticCommands = false;
            for (auto& module : this->modules) {
                for (auto& command : module.getCommands()) {
                    if (!command.isMarkovian()) {
                        command.setMarkovian(true);
                        hasProbabilisticCommands = true;
                    }
                }
            }
            STORM_LOG_WARN_COND(!hasProbabilisticCommands,
                                "The input model is a CTMC, but uses probabilistic commands like they are used in PRISM. Consider rewriting the commands to "
                                "use Markovian commands instead.");
        }
        // Then check the validity.
        this->checkValidity(Program::ValidityCheckLevel::VALIDINPUT);
    }
}

Program::ModelType Program::getModelType() const {
    return modelType;
}

bool Program::isDiscreteTimeModel() const {
    return modelType == ModelType::DTMC || modelType == ModelType::MDP || modelType == ModelType::POMDP || modelType == ModelType::SMG;
}

bool Program::isDeterministicModel() const {
    return modelType == ModelType::DTMC || modelType == ModelType::CTMC;
}

bool Program::isPartiallyObservable() const {
    return modelType == ModelType::POMDP;
}

size_t Program::getNumberOfCommands() const {
    size_t res = 0;
    for (auto const& module : this->getModules()) {
        res += module.getNumberOfCommands();
    }
    return res;
}

bool Program::hasUnboundedVariables() const {
    for (auto const& globalIntegerVariable : this->globalIntegerVariables) {
        if (!globalIntegerVariable.hasLowerBoundExpression() || !globalIntegerVariable.hasUpperBoundExpression()) {
            return true;
        }
    }
    for (auto const& module : modules) {
        if (module.hasUnboundedVariables()) {
            return true;
        }
    }
    return false;
}

bool Program::hasUndefinedConstants() const {
    for (auto const& constant : this->getConstants()) {
        if (!constant.isDefined()) {
            return true;
        }
    }
    return false;
}

bool Program::undefinedConstantsAreGraphPreserving() const {
    if (!this->hasUndefinedConstants()) {
        return true;
    }

    // Gather the variables of all undefined constants.
    std::set<storm::expressions::Variable> undefinedConstantVariables;
    for (auto const& constant : this->getConstants()) {
        if (!constant.isDefined()) {
            undefinedConstantVariables.insert(constant.getExpressionVariable());
        }
    }

    // Start by checking the defining expressions of all defined constants. If it contains a currently undefined
    // constant, we need to mark the target constant as undefined as well.
    for (auto const& constant : this->getConstants()) {
        if (constant.isDefined()) {
            if (constant.getExpression().containsVariable(undefinedConstantVariables)) {
                undefinedConstantVariables.insert(constant.getExpressionVariable());
            }
        }
    }

    // Now check initial value and range expressions of global variables.
    for (auto const& booleanVariable : this->getGlobalBooleanVariables()) {
        if (booleanVariable.hasInitialValue()) {
            if (booleanVariable.getInitialValueExpression().containsVariable(undefinedConstantVariables)) {
                return false;
            }
        }
    }
    for (auto const& integerVariable : this->getGlobalIntegerVariables()) {
        if (integerVariable.hasInitialValue()) {
            if (integerVariable.getInitialValueExpression().containsVariable(undefinedConstantVariables)) {
                return false;
            }
        }
        if (integerVariable.hasLowerBoundExpression() && integerVariable.getLowerBoundExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
        if (integerVariable.hasUpperBoundExpression() && integerVariable.getUpperBoundExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }

    // Proceed by checking each of the modules.
    for (auto const& module : this->getModules()) {
        if (!module.containsVariablesOnlyInUpdateProbabilities(undefinedConstantVariables)) {
            return false;
        }
    }

    // Check the reward models.
    for (auto const& rewardModel : this->getRewardModels()) {
        rewardModel.containsVariablesOnlyInRewardValueExpressions(undefinedConstantVariables);
    }

    // Initial construct.
    if (this->hasInitialConstruct()) {
        if (this->getInitialConstruct().getInitialStatesExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }

    // Labels.
    for (auto const& label : this->getLabels()) {
        if (label.getStatePredicateExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }

    return true;
}

std::vector<std::reference_wrapper<storm::prism::Constant const>> Program::getUndefinedConstants() const {
    std::vector<std::reference_wrapper<storm::prism::Constant const>> result;
    for (auto const& constant : this->getConstants()) {
        if (!constant.isDefined()) {
            result.push_back(constant);
        }
    }
    return result;
}

std::string Program::getUndefinedConstantsAsString() const {
    std::stringstream stream;
    bool printComma = false;
    for (auto const& constant : getUndefinedConstants()) {
        if (printComma) {
            stream << ", ";
        } else {
            printComma = true;
        }
        stream << constant.get().getName() << " (" << constant.get().getType() << ")";
    }
    stream << ".";
    return stream.str();
}

bool Program::hasConstant(std::string const& constantName) const {
    return this->constantToIndexMap.find(constantName) != this->constantToIndexMap.end();
}

Constant const& Program::getConstant(std::string const& constantName) const {
    auto const& constantIndexPair = this->constantToIndexMap.find(constantName);
    return this->getConstants()[constantIndexPair->second];
}

std::vector<Constant> const& Program::getConstants() const {
    return this->constants;
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Program::getConstantsSubstitution() const {
    return getConstantsFormulasSubstitution(true, false);
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Program::getFormulasSubstitution() const {
    return getConstantsFormulasSubstitution(false, true);
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Program::getConstantsFormulasSubstitution(bool getConstantsSubstitution,
                                                                                                                 bool getFormulasSubstitution) const {
    std::map<storm::expressions::Variable, storm::expressions::Expression> result;
    if (getConstantsSubstitution) {
        for (auto const& constant : this->getConstants()) {
            if (constant.isDefined()) {
                result.emplace(constant.getExpressionVariable(), constant.getExpression().substitute(result));
            }
        }
    }
    if (getFormulasSubstitution) {
        for (auto const& formula : this->getFormulas()) {
            result.emplace(formula.getExpressionVariable(), formula.getExpression().substitute(result));
        }
    }
    return result;
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Program::getSubstitutionForRenamedModule(
    Module const& renamedModule, std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    auto renaming = getFinalRenamingOfModule(renamedModule);
    std::map<storm::expressions::Variable, storm::expressions::Expression> renamingAsSubstitution;
    for (auto const& renamingPair : renaming) {
        if (getManager().hasVariable(renamingPair.first)) {
            assert(getManager().hasVariable(renamingPair.second));
            renamingAsSubstitution.emplace(getManager().getVariable(renamingPair.first), getManager().getVariableExpression(renamingPair.second));
        }
    }

    std::map<storm::expressions::Variable, storm::expressions::Expression> newSubstitution;
    for (auto const& substVarExpr : substitution) {
        newSubstitution.emplace(substVarExpr.first, storm::jani::substituteJaniExpression(substVarExpr.second, renamingAsSubstitution, false));
    }
    return newSubstitution;
}

std::map<std::string, std::string> Program::getFinalRenamingOfModule(Module const& renamedModule) const {
    std::vector<Module const*> moduleStack = {&renamedModule};
    while (moduleStack.back()->isRenamedFromModule()) {
        moduleStack.push_back(&getModule(moduleStack.back()->getBaseModule()));
    }

    assert(!moduleStack.back()->isRenamedFromModule());
    moduleStack.pop_back();
    assert(moduleStack.empty() || moduleStack.back()->isRenamedFromModule());
    std::map<std::string, std::string> currentRenaming;
    while (!moduleStack.empty()) {
        Module const& currentModule = *moduleStack.back();
        moduleStack.pop_back();
        assert(currentModule.isRenamedFromModule());
        std::map<std::string, std::string> newRenaming = currentModule.getRenaming();
        for (auto const& renaimingPair : newRenaming) {
            auto findRes = currentRenaming.find(renaimingPair.second);
            if (findRes != currentRenaming.end()) {
                newRenaming[renaimingPair.second] = findRes->second;
                currentRenaming.erase(findRes);
            }
        }
        newRenaming.insert(currentRenaming.begin(), currentRenaming.end());
        currentRenaming = std::move(newRenaming);
    }
    return currentRenaming;
}

std::size_t Program::getNumberOfConstants() const {
    return this->getConstants().size();
}

std::vector<BooleanVariable> const& Program::getGlobalBooleanVariables() const {
    return this->globalBooleanVariables;
}

std::vector<IntegerVariable> const& Program::getGlobalIntegerVariables() const {
    return this->globalIntegerVariables;
}

std::set<storm::expressions::Variable> Program::getAllExpressionVariables(bool includeConstants) const {
    std::set<storm::expressions::Variable> result;

    if (includeConstants) {
        for (auto const& constant : constants) {
            result.insert(constant.getExpressionVariable());
        }
    }
    for (auto const& variable : globalBooleanVariables) {
        result.insert(variable.getExpressionVariable());
    }
    for (auto const& variable : globalIntegerVariables) {
        result.insert(variable.getExpressionVariable());
    }
    for (auto const& module : modules) {
        auto const& moduleVariables = module.getAllExpressionVariables();
        result.insert(moduleVariables.begin(), moduleVariables.end());
    }

    return result;
}

std::vector<storm::expressions::Expression> Program::getAllRangeExpressions() const {
    std::vector<storm::expressions::Expression> result;
    for (auto const& globalIntegerVariable : this->globalIntegerVariables) {
        if (globalIntegerVariable.hasLowerBoundExpression() || globalIntegerVariable.hasUpperBoundExpression()) {
            result.push_back(globalIntegerVariable.getRangeExpression());
        }
    }

    for (auto const& module : modules) {
        std::vector<storm::expressions::Expression> moduleRangeExpressions = module.getAllRangeExpressions();
        result.insert(result.end(), moduleRangeExpressions.begin(), moduleRangeExpressions.end());
    }

    return result;
}

bool Program::globalBooleanVariableExists(std::string const& variableName) const {
    return this->globalBooleanVariableToIndexMap.count(variableName) > 0;
}

bool Program::globalIntegerVariableExists(std::string const& variableName) const {
    return this->globalIntegerVariableToIndexMap.count(variableName) > 0;
}

BooleanVariable const& Program::getGlobalBooleanVariable(std::string const& variableName) const {
    auto const& nameIndexPair = this->globalBooleanVariableToIndexMap.find(variableName);
    STORM_LOG_THROW(nameIndexPair != this->globalBooleanVariableToIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Unknown boolean variable '" << variableName << "'.");
    return this->getGlobalBooleanVariables()[nameIndexPair->second];
}

IntegerVariable const& Program::getGlobalIntegerVariable(std::string const& variableName) const {
    auto const& nameIndexPair = this->globalIntegerVariableToIndexMap.find(variableName);
    STORM_LOG_THROW(nameIndexPair != this->globalIntegerVariableToIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Unknown integer variable '" << variableName << "'.");
    return this->getGlobalIntegerVariables()[nameIndexPair->second];
}

std::size_t Program::getNumberOfGlobalBooleanVariables() const {
    return this->getGlobalBooleanVariables().size();
}

std::size_t Program::getNumberOfGlobalIntegerVariables() const {
    return this->getGlobalIntegerVariables().size();
}

std::vector<Formula> const& Program::getFormulas() const {
    return this->formulas;
}

std::vector<Player> const& Program::getPlayers() const {
    return this->players;
}

std::size_t Program::getNumberOfPlayers() const {
    return this->getPlayers().size();
}

storm::storage::PlayerIndex const& Program::getIndexOfPlayer(std::string const& playerName) const {
    return this->playerToIndexMap.at(playerName);
}

std::map<std::string, storm::storage::PlayerIndex> const& Program::getPlayerNameToIndexMapping() const {
    return playerToIndexMap;
}

std::vector<storm::storage::PlayerIndex> Program::buildModuleIndexToPlayerIndexMap() const {
    std::vector<storm::storage::PlayerIndex> result(this->getModules().size(), storm::storage::INVALID_PLAYER_INDEX);
    for (storm::storage::PlayerIndex i = 0; i < this->getPlayers().size(); ++i) {
        for (auto const& module : this->getPlayers()[i].getModules()) {
            STORM_LOG_ASSERT(hasModule(module), "Module " << module << " not found.");
            STORM_LOG_ASSERT(moduleToIndexMap.at(module) < this->getModules().size(), "module index " << moduleToIndexMap.at(module) << " out of range.");
            result[moduleToIndexMap.at(module)] = i;
        }
    }
    return result;
}

std::map<uint64_t, storm::storage::PlayerIndex> Program::buildActionIndexToPlayerIndexMap() const {
    std::map<uint64_t, storm::storage::PlayerIndex> result;
    // First insert an invalid player index for all available actions
    for (auto const& action : indexToActionMap) {
        result.emplace_hint(result.end(), action.first, storm::storage::INVALID_PLAYER_INDEX);
    }
    // Now set the actual player indices.
    // Note that actions that are not assigned to a player will still have INVALID_PLAYER_INDEX afterwards
    for (storm::storage::PlayerIndex i = 0; i < this->getPlayers().size(); ++i) {
        for (auto const& act : this->getPlayers()[i].getActions()) {
            STORM_LOG_ASSERT(hasAction(act), "Action " << act << " not found.");
            result[actionToIndexMap.at(act)] = i;
        }
    }
    return result;
}

std::size_t Program::getNumberOfFormulas() const {
    return this->getFormulas().size();
}

std::size_t Program::getNumberOfModules() const {
    return this->getModules().size();
}

storm::prism::Module const& Program::getModule(uint_fast64_t index) const {
    return this->modules[index];
}

bool Program::hasModule(std::string const& moduleName) const {
    return this->moduleToIndexMap.find(moduleName) != this->moduleToIndexMap.end();
}

Module const& Program::getModule(std::string const& moduleName) const {
    auto const& nameIndexPair = this->moduleToIndexMap.find(moduleName);
    STORM_LOG_THROW(nameIndexPair != this->moduleToIndexMap.end(), storm::exceptions::OutOfRangeException, "Unknown module '" << moduleName << "'.");
    return this->getModules()[nameIndexPair->second];
}

std::vector<storm::prism::Module> const& Program::getModules() const {
    return this->modules;
}

std::map<std::string, uint_fast64_t> const& Program::getActionNameToIndexMapping() const {
    return actionToIndexMap;
}

uint64_t Program::getNumberOfUnlabeledCommands() const {
    uint64_t result = 0;
    for (auto const& m : modules) {
        result += m.getNumberOfUnlabeledCommands();
    }
    return result;
}

bool Program::hasInitialConstruct() const {
    return static_cast<bool>(initialConstruct);
}

storm::prism::InitialConstruct const& Program::getInitialConstruct() const {
    return this->initialConstruct.get();
}

boost::optional<InitialConstruct> const& Program::getOptionalInitialConstruct() const {
    return this->initialConstruct;
}

void Program::updateInitialStatesExpression(expressions::Expression const& newExpression) {
    STORM_LOG_THROW(hasInitialConstruct(), exceptions::InvalidOperationException, "We can only update the initial construct, if it already exists.");
    this->initialConstruct = boost::make_optional(InitialConstruct(newExpression));
}

storm::expressions::Expression Program::getInitialStatesExpression() const {
    // If there is an initial construct, return its expression. If not, we construct the expression from the
    // initial values of the variables (which have to exist).
    if (this->hasInitialConstruct()) {
        return this->getInitialConstruct().getInitialStatesExpression();
    } else {
        storm::expressions::Expression result;

        for (auto const& variable : this->getGlobalBooleanVariables()) {
            if (result.isInitialized()) {
                result = result && storm::expressions::iff(variable.getExpressionVariable(), variable.getInitialValueExpression());
            } else {
                result = storm::expressions::iff(variable.getExpressionVariable(), variable.getInitialValueExpression());
            }
        }
        for (auto const& variable : this->getGlobalIntegerVariables()) {
            if (result.isInitialized()) {
                result = result && variable.getExpressionVariable() == variable.getInitialValueExpression();
            } else {
                result = variable.getExpressionVariable() == variable.getInitialValueExpression();
            }
        }
        for (auto const& module : this->getModules()) {
            for (auto const& variable : module.getBooleanVariables()) {
                if (result.isInitialized()) {
                    result = result && storm::expressions::iff(variable.getExpressionVariable(), variable.getInitialValueExpression());
                } else {
                    result = storm::expressions::iff(variable.getExpressionVariable(), variable.getInitialValueExpression());
                }
            }
            for (auto const& variable : module.getIntegerVariables()) {
                if (result.isInitialized()) {
                    result = result && variable.getExpressionVariable() == variable.getInitialValueExpression();
                } else {
                    result = variable.getExpressionVariable() == variable.getInitialValueExpression();
                }
            }
        }

        // If there are no variables, there is no restriction on the initial states.
        if (!result.isInitialized()) {
            result = manager->boolean(true);
        }

        return result;
    }
}

bool Program::hasIntervalUpdates() const {
    for (auto const& module : this->modules) {
        for (auto const& command : module.getCommands()) {
            for (auto const& update : command.getUpdates()) {
                if (update.isLikelihoodInterval()) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Program::specifiesSystemComposition() const {
    return static_cast<bool>(systemCompositionConstruct);
}

SystemCompositionConstruct const& Program::getSystemCompositionConstruct() const {
    return systemCompositionConstruct.get();
}

boost::optional<SystemCompositionConstruct> Program::getOptionalSystemCompositionConstruct() const {
    return systemCompositionConstruct;
}

std::shared_ptr<Composition> Program::getDefaultSystemComposition() const {
    std::shared_ptr<Composition> current = std::make_shared<ModuleComposition>(this->modules.front().getName());

    for (uint_fast64_t index = 1; index < this->modules.size(); ++index) {
        std::shared_ptr<Composition> newComposition =
            std::make_shared<SynchronizingParallelComposition>(current, std::make_shared<ModuleComposition>(this->modules[index].getName()));
        current = newComposition;
    }

    return current;
}

std::set<std::string> const& Program::getActions() const {
    return this->actions;
}

std::set<uint_fast64_t> const& Program::getSynchronizingActionIndices() const {
    return this->synchronizingActionIndices;
}

std::string const& Program::getActionName(uint_fast64_t actionIndex) const {
    auto const& indexNamePair = this->indexToActionMap.find(actionIndex);
    STORM_LOG_THROW(indexNamePair != this->indexToActionMap.end(), storm::exceptions::InvalidArgumentException, "Unknown action index " << actionIndex << ".");
    return indexNamePair->second;
}

uint_fast64_t Program::getActionIndex(std::string const& actionName) const {
    auto const& nameIndexPair = this->actionToIndexMap.find(actionName);
    STORM_LOG_THROW(nameIndexPair != this->actionToIndexMap.end(), storm::exceptions::InvalidArgumentException, "Unknown action name '" << actionName << "'.");
    return nameIndexPair->second;
}

bool Program::hasAction(std::string const& actionName) const {
    return this->actionToIndexMap.find(actionName) != this->actionToIndexMap.end();
}

bool Program::hasAction(uint_fast64_t const& actionIndex) const {
    return this->indexToActionMap.find(actionIndex) != this->indexToActionMap.end();
}

std::set<uint_fast64_t> const& Program::getModuleIndicesByAction(std::string const& action) const {
    auto const& nameIndexPair = this->actionToIndexMap.find(action);
    STORM_LOG_THROW(nameIndexPair != this->actionToIndexMap.end(), storm::exceptions::OutOfRangeException, "Action name '" << action << "' does not exist.");
    return this->getModuleIndicesByActionIndex(nameIndexPair->second);
}

std::set<uint_fast64_t> const& Program::getModuleIndicesByActionIndex(uint_fast64_t actionIndex) const {
    auto const& actionModuleSetPair = this->actionIndicesToModuleIndexMap.find(actionIndex);
    STORM_LOG_THROW(actionModuleSetPair != this->actionIndicesToModuleIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Action with index '" << actionIndex << "' does not exist.");
    return actionModuleSetPair->second;
}

uint_fast64_t Program::getModuleIndexByVariable(std::string const& variableName) const {
    auto const& variableNameToModuleIndexPair = this->variableToModuleIndexMap.find(variableName);
    STORM_LOG_THROW(variableNameToModuleIndexPair != this->variableToModuleIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Variable '" << variableName << "' does not exist.");
    return variableNameToModuleIndexPair->second;
}

std::pair<uint_fast64_t, uint_fast64_t> Program::getModuleCommandIndexByGlobalCommandIndex(uint_fast64_t globalCommandIndex) const {
    uint_fast64_t moduleIndex = 0;
    for (auto const& module : modules) {
        uint_fast64_t commandIndex = 0;
        for (auto const& command : module.getCommands()) {
            if (command.getGlobalIndex() == globalCommandIndex) {
                return std::pair<uint_fast64_t, uint_fast64_t>(moduleIndex, commandIndex);
            }
            ++commandIndex;
        }
        ++moduleIndex;
    }
    // This point should not be reached if the globalCommandIndex is valid
    STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException, "Global command index '" << globalCommandIndex << "' does not exist.");
    return std::pair<uint_fast64_t, uint_fast64_t>(0, 0);
}

bool Program::hasRewardModel() const {
    return !this->rewardModels.empty();
}

bool Program::hasRewardModel(std::string const& name) const {
    auto const& nameIndexPair = this->rewardModelToIndexMap.find(name);
    return nameIndexPair != this->rewardModelToIndexMap.end();
}

std::vector<storm::prism::RewardModel> const& Program::getRewardModels() const {
    return this->rewardModels;
}

std::size_t Program::getNumberOfRewardModels() const {
    return this->getRewardModels().size();
}

storm::prism::RewardModel const& Program::getRewardModel(std::string const& name) const {
    auto const& nameIndexPair = this->rewardModelToIndexMap.find(name);
    STORM_LOG_THROW(nameIndexPair != this->rewardModelToIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Reward model '" << name << "' does not exist.");
    return this->getRewardModels()[nameIndexPair->second];
}

RewardModel const& Program::getRewardModel(uint_fast64_t index) const {
    STORM_LOG_THROW(this->getNumberOfRewardModels() > index, storm::exceptions::OutOfRangeException, "Reward model with index " << index << " does not exist.");
    return this->rewardModels[index];
}

bool Program::hasLabel(std::string const& labelName) const {
    auto it = std::find_if(labels.begin(), labels.end(), [&labelName](storm::prism::Label const& label) { return label.getName() == labelName; });
    return it != labels.end();
}

std::vector<Label> const& Program::getLabels() const {
    return this->labels;
}

std::vector<storm::expressions::Expression> Program::getAllGuards(bool negated) const {
    std::vector<storm::expressions::Expression> allGuards;
    for (auto const& module : modules) {
        for (auto const& command : module.getCommands()) {
            allGuards.push_back(negated ? !command.getGuardExpression() : command.getGuardExpression());
        }
    }
    return allGuards;
}

storm::expressions::Expression const& Program::getLabelExpression(std::string const& label) const {
    auto const& labelIndexPair = labelToIndexMap.find(label);
    STORM_LOG_THROW(labelIndexPair != labelToIndexMap.end(), storm::exceptions::InvalidArgumentException,
                    "Cannot retrieve expression for unknown label '" << label << "'.");
    return this->labels[labelIndexPair->second].getStatePredicateExpression();
}

std::map<std::string, storm::expressions::Expression> Program::getLabelToExpressionMapping() const {
    std::map<std::string, storm::expressions::Expression> result;
    for (auto const& label : labels) {
        result.emplace(label.getName(), label.getStatePredicateExpression());
    }
    return result;
}

std::size_t Program::getNumberOfLabels() const {
    return this->getLabels().size();
}

void Program::addLabel(std::string const& name, storm::expressions::Expression const& statePredicateExpression) {
    auto it = std::find_if(this->labels.begin(), this->labels.end(), [&name](storm::prism::Label const& label) { return label.getName() == name; });
    STORM_LOG_THROW(it == this->labels.end(), storm::exceptions::InvalidArgumentException,
                    "Cannot add a label '" << name << "', because a label with that name already exists.");
    this->labels.emplace_back(name, statePredicateExpression);
}

void Program::removeLabel(std::string const& name) {
    auto it = std::find_if(this->labels.begin(), this->labels.end(), [&name](storm::prism::Label const& label) { return label.getName() == name; });
    STORM_LOG_THROW(it != this->labels.end(), storm::exceptions::InvalidArgumentException, "Canno remove unknown label '" << name << "'.");
    this->labels.erase(it);
}

void Program::removeRewardModels() {
    this->rewardModels.clear();
    this->rewardModelToIndexMap.clear();
}

void Program::filterLabels(std::set<std::string> const& labelSet) {
    std::vector<storm::prism::Label> newLabels;
    newLabels.reserve(labelSet.size());

    // Now filter the labels by the criterion whether or not their name appears in the given label set.
    for (auto it = labels.begin(), ite = labels.end(); it != ite; ++it) {
        auto setIt = labelSet.find(it->getName());
        if (setIt != labelSet.end()) {
            newLabels.emplace_back(*it);
        }
    }

    // Move the new labels in place.
    this->labels = std::move(newLabels);
}

std::vector<ObservationLabel> const& Program::getObservationLabels() const {
    return this->observationLabels;
}

std::size_t Program::getNumberOfObservationLabels() const {
    return this->observationLabels.size();
}

storm::storage::BitVector const& Program::getPossiblySynchronizingCommands() const {
    return possiblySynchronizingCommands;
}

Program Program::restrictCommands(storm::storage::FlatSet<uint_fast64_t> const& indexSet) const {
    std::vector<storm::prism::Module> newModules;
    newModules.reserve(this->getNumberOfModules());

    for (auto const& module : this->getModules()) {
        newModules.push_back(module.restrictCommands(indexSet));
    }

    return Program(this->manager, this->getModelType(), this->getConstants(), this->getGlobalBooleanVariables(), this->getGlobalIntegerVariables(),
                   this->getFormulas(), this->getPlayers(), newModules, this->getActionNameToIndexMapping(), this->getRewardModels(), this->getLabels(),
                   this->getObservationLabels(), this->getOptionalInitialConstruct(), this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

void Program::createMappings() {
    // Build the mappings for constants, global variables, formulas, modules, reward models and labels.
    for (uint_fast64_t constantIndex = 0; constantIndex < this->getNumberOfConstants(); ++constantIndex) {
        this->constantToIndexMap[this->getConstants()[constantIndex].getName()] = constantIndex;
    }
    for (uint_fast64_t globalVariableIndex = 0; globalVariableIndex < this->getNumberOfGlobalBooleanVariables(); ++globalVariableIndex) {
        this->globalBooleanVariableToIndexMap[this->getGlobalBooleanVariables()[globalVariableIndex].getName()] = globalVariableIndex;
    }
    for (uint_fast64_t globalVariableIndex = 0; globalVariableIndex < this->getNumberOfGlobalIntegerVariables(); ++globalVariableIndex) {
        this->globalIntegerVariableToIndexMap[this->getGlobalIntegerVariables()[globalVariableIndex].getName()] = globalVariableIndex;
    }
    for (uint_fast64_t formulaIndex = 0; formulaIndex < this->getNumberOfFormulas(); ++formulaIndex) {
        this->formulaToIndexMap[this->getFormulas()[formulaIndex].getName()] = formulaIndex;
    }
    for (uint_fast64_t labelIndex = 0; labelIndex < this->getNumberOfLabels(); ++labelIndex) {
        this->labelToIndexMap[this->getLabels()[labelIndex].getName()] = labelIndex;
    }
    for (uint_fast64_t moduleIndex = 0; moduleIndex < this->getNumberOfModules(); ++moduleIndex) {
        this->moduleToIndexMap[this->getModules()[moduleIndex].getName()] = moduleIndex;
    }
    for (storm::storage::PlayerIndex playerIndex = 0; playerIndex < this->getNumberOfPlayers(); ++playerIndex) {
        this->playerToIndexMap[this->getPlayers()[playerIndex].getName()] = playerIndex;
    }
    for (uint_fast64_t rewardModelIndex = 0; rewardModelIndex < this->getNumberOfRewardModels(); ++rewardModelIndex) {
        this->rewardModelToIndexMap[this->getRewardModels()[rewardModelIndex].getName()] = rewardModelIndex;
    }

    for (auto const& actionIndexPair : this->getActionNameToIndexMapping()) {
        this->actions.insert(actionIndexPair.first);
        this->indexToActionMap.emplace(actionIndexPair.second, actionIndexPair.first);

        // Only let all non-zero indices be synchronizing.
        if (actionIndexPair.second != 0) {
            this->synchronizingActionIndices.insert(actionIndexPair.second);
            this->actionIndicesToModuleIndexMap[actionIndexPair.second] = std::set<uint_fast64_t>();
        }
    }

    // Build the mapping from action names to module indices so that the lookup can later be performed quickly.
    for (unsigned int moduleIndex = 0; moduleIndex < this->getNumberOfModules(); moduleIndex++) {
        Module const& module = this->getModule(moduleIndex);

        for (auto const& actionIndex : module.getSynchronizingActionIndices()) {
            this->actionIndicesToModuleIndexMap[actionIndex].insert(moduleIndex);
        }

        // Put in the appropriate entries for the mapping from variable names to module index.
        for (auto const& booleanVariable : module.getBooleanVariables()) {
            this->variableToModuleIndexMap[booleanVariable.getName()] = moduleIndex;
        }
        for (auto const& integerVariable : module.getIntegerVariables()) {
            this->variableToModuleIndexMap[integerVariable.getName()] = moduleIndex;
        }
        for (auto const& clockVariable : module.getClockVariables()) {
            this->variableToModuleIndexMap[clockVariable.getName()] = moduleIndex;
        }
    }
}

Program Program::defineUndefinedConstants(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const {
    // For sanity checking, we keep track of all undefined constants that we define in the course of this procedure.
    std::set<storm::expressions::Variable> definedUndefinedConstants;

    std::vector<Constant> newConstants;
    newConstants.reserve(this->getNumberOfConstants());
    for (auto const& constant : this->getConstants()) {
        // If the constant is already defined, we need to replace the appearances of undefined constants in its
        // defining expression
        if (constant.isDefined()) {
            // Make sure we are not trying to define an already defined constant.
            STORM_LOG_THROW(constantDefinitions.find(constant.getExpressionVariable()) == constantDefinitions.end(),
                            storm::exceptions::InvalidArgumentException, "Illegally defining already defined constant '" << constant.getName() << "'.");

            // Now replace the occurrences of undefined constants in its defining expression.
            newConstants.emplace_back(constant.getExpressionVariable(), constant.getExpression().substitute(constantDefinitions), constant.getFilename(),
                                      constant.getLineNumber());
        } else {
            auto const& variableExpressionPair = constantDefinitions.find(constant.getExpressionVariable());

            // If the constant is not defined by the mapping, we leave it like it is.
            if (variableExpressionPair == constantDefinitions.end()) {
                newConstants.emplace_back(constant);
            } else {
                // Otherwise, we add it to the defined constants and assign it the appropriate expression.
                definedUndefinedConstants.insert(constant.getExpressionVariable());

                // Make sure the type of the constant is correct.
                STORM_LOG_THROW(variableExpressionPair->second.getType() == constant.getType(), storm::exceptions::InvalidArgumentException,
                                "Illegal type of expression defining constant '" << constant.getName() << "'.");

                // Now create the defined constant.
                newConstants.emplace_back(constant.getExpressionVariable(), variableExpressionPair->second, constant.getFilename(), constant.getLineNumber());
            }
        }
    }

    return Program(this->manager, this->getModelType(), newConstants, this->getGlobalBooleanVariables(), this->getGlobalIntegerVariables(), this->getFormulas(),
                   this->getPlayers(), this->getModules(), this->getActionNameToIndexMapping(), this->getRewardModels(), this->getLabels(),
                   this->getObservationLabels(), this->getOptionalInitialConstruct(), this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::substituteConstants() const {
    return substituteConstantsFormulas(true, false);
}

Program Program::substituteFormulas() const {
    return substituteConstantsFormulas(false, true);
}

Program Program::substituteNonStandardPredicates() const {
    // TODO support in constants,  initial construct, and rewards

    std::vector<Formula> newFormulas;
    newFormulas.reserve(this->getNumberOfFormulas());
    for (auto const& oldFormula : this->getFormulas()) {
        newFormulas.emplace_back(oldFormula.substituteNonStandardPredicates());
    }

    std::vector<BooleanVariable> newBooleanVariables;
    newBooleanVariables.reserve(this->getNumberOfGlobalBooleanVariables());
    for (auto const& booleanVariable : this->getGlobalBooleanVariables()) {
        newBooleanVariables.emplace_back(booleanVariable.substituteNonStandardPredicates());
    }

    std::vector<IntegerVariable> newIntegerVariables;
    newBooleanVariables.reserve(this->getNumberOfGlobalIntegerVariables());
    for (auto const& integerVariable : this->getGlobalIntegerVariables()) {
        newIntegerVariables.emplace_back(integerVariable.substituteNonStandardPredicates());
    }

    std::vector<Module> newModules;
    newModules.reserve(this->getNumberOfModules());
    for (auto const& module : this->getModules()) {
        newModules.emplace_back(module.substituteNonStandardPredicates());
    }

    std::vector<Label> newLabels;
    newLabels.reserve(this->getNumberOfLabels());
    for (auto const& label : this->getLabels()) {
        newLabels.emplace_back(label.substituteNonStandardPredicates());
    }

    std::vector<ObservationLabel> newObservationLabels;
    newObservationLabels.reserve(this->getNumberOfObservationLabels());
    for (auto const& label : this->getObservationLabels()) {
        newObservationLabels.emplace_back(label.substituteNonStandardPredicates());
    }

    return Program(this->manager, this->getModelType(), this->getConstants(), newBooleanVariables, newIntegerVariables, newFormulas, this->getPlayers(),
                   newModules, this->getActionNameToIndexMapping(), this->getRewardModels(), newLabels, newObservationLabels, initialConstruct,
                   this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::substituteConstantsFormulas(bool substituteConstants, bool substituteFormulas) const {
    // Formulas need to be substituted first. otherwise, constants appearing in formula expressions can not be handled properly
    if (substituteConstants && substituteFormulas) {
        return this->substituteFormulas().substituteConstants();
    }

    // We start by creating the appropriate substitution.
    std::map<storm::expressions::Variable, storm::expressions::Expression> substitution =
        getConstantsFormulasSubstitution(substituteConstants, substituteFormulas);

    std::vector<Constant> newConstants;
    newConstants.reserve(this->getNumberOfConstants());
    for (auto const& oldConstant : this->getConstants()) {
        newConstants.push_back(oldConstant.substitute(substitution));
    }

    std::vector<Formula> newFormulas;
    newFormulas.reserve(this->getNumberOfFormulas());
    for (auto const& oldFormula : this->getFormulas()) {
        newFormulas.emplace_back(oldFormula.substitute(substitution));
    }

    std::vector<BooleanVariable> newBooleanVariables;
    newBooleanVariables.reserve(this->getNumberOfGlobalBooleanVariables());
    for (auto const& booleanVariable : this->getGlobalBooleanVariables()) {
        newBooleanVariables.emplace_back(booleanVariable.substitute(substitution));
    }

    std::vector<IntegerVariable> newIntegerVariables;
    newBooleanVariables.reserve(this->getNumberOfGlobalIntegerVariables());
    for (auto const& integerVariable : this->getGlobalIntegerVariables()) {
        newIntegerVariables.emplace_back(integerVariable.substitute(substitution));
    }

    std::vector<Module> newModules;
    newModules.reserve(this->getNumberOfModules());
    for (auto const& module : this->getModules()) {
        if (module.isRenamedFromModule()) {
            // The renaming needs to be applied to the substitution as well.
            auto renamedSubstitution = getSubstitutionForRenamedModule(module, substitution);
            newModules.emplace_back(module.substitute(renamedSubstitution));
        } else {
            newModules.emplace_back(module.substitute(substitution));
        }
    }

    std::vector<RewardModel> newRewardModels;
    newRewardModels.reserve(this->getNumberOfRewardModels());
    for (auto const& rewardModel : this->getRewardModels()) {
        newRewardModels.emplace_back(rewardModel.substitute(substitution));
    }

    boost::optional<storm::prism::InitialConstruct> newInitialConstruct;
    if (this->hasInitialConstruct()) {
        newInitialConstruct = this->getInitialConstruct().substitute(substitution);
    }

    std::vector<Label> newLabels;
    newLabels.reserve(this->getNumberOfLabels());
    for (auto const& label : this->getLabels()) {
        newLabels.emplace_back(label.substitute(substitution));
    }

    std::vector<ObservationLabel> newObservationLabels;
    newObservationLabels.reserve(this->getNumberOfObservationLabels());
    for (auto const& label : this->getObservationLabels()) {
        newObservationLabels.emplace_back(label.substitute(substitution));
    }

    return Program(this->manager, this->getModelType(), newConstants, newBooleanVariables, newIntegerVariables, newFormulas, this->getPlayers(), newModules,
                   this->getActionNameToIndexMapping(), newRewardModels, newLabels, newObservationLabels, newInitialConstruct,
                   this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::preprocess(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const {
    return this->defineUndefinedConstants(constantDefinitions).substituteConstantsFormulas().substituteNonStandardPredicates();
}

Program Program::preprocess(std::string const& constantDefinitionString) const {
    return this->preprocess(storm::storage::parseConstantDefinitionString(this->getManager(), constantDefinitionString));
}

Program Program::labelUnlabelledCommands(std::map<uint64_t, std::string> const& nameSuggestions) const {
    for (auto const& entry : nameSuggestions) {
        STORM_LOG_THROW(!hasAction(entry.second), storm::exceptions::InvalidArgumentException, "Cannot suggest names already in the program.");
    }
    std::vector<Module> newModules;
    std::vector<RewardModel> newRewardModels;
    std::map<std::string, uint64_t> newActionNameToIndexMapping = getActionNameToIndexMapping();

    uint64_t oldId = 1;
    if (!getSynchronizingActionIndices().empty()) {
        oldId = *(getSynchronizingActionIndices().rbegin()) + 1;
    }
    uint64_t newId = oldId;
    for (auto const& module : modules) {
        newModules.push_back(module.labelUnlabelledCommands(nameSuggestions, newId, newActionNameToIndexMapping));
    }

    std::vector<std::pair<uint64_t, std::string>> newActionNames;
    for (auto const& entry : newActionNameToIndexMapping) {
        if (!hasAction(entry.first)) {
            newActionNames.emplace_back(entry.second, entry.first);
        }
    }
    for (auto const& rewardModel : rewardModels) {
        newRewardModels.push_back(rewardModel.labelUnlabelledCommands(newActionNames));
    }

    return Program(this->manager, this->getModelType(), this->getConstants(), this->getGlobalBooleanVariables(), this->getGlobalIntegerVariables(),
                   this->getFormulas(), this->getPlayers(), newModules, newActionNameToIndexMapping, newRewardModels, this->getLabels(),
                   this->getObservationLabels(), this->getOptionalInitialConstruct(), this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::replaceVariableInitializationByInitExpression() const {
    std::vector<BooleanVariable> newBooleanVariables = globalBooleanVariables;
    for (auto& newVar : newBooleanVariables) {
        newVar.setInitialValueExpression(storm::expressions::Expression());
    }

    std::vector<IntegerVariable> newIntegerVariables = globalIntegerVariables;
    for (auto& newVar : newIntegerVariables) {
        newVar.setInitialValueExpression(storm::expressions::Expression());
    }

    std::vector<Module> newModules = this->getModules();
    for (auto& module : newModules) {
        module.removeVariableInitialization();
    }

    return Program(this->manager, this->getModelType(), this->getConstants(), newBooleanVariables, newIntegerVariables, this->getFormulas(), this->getPlayers(),
                   newModules, this->actionToIndexMap, this->getRewardModels(), this->getLabels(), this->getObservationLabels(),
                   InitialConstruct(this->getInitialStatesExpression()), this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::replaceConstantByVariable(Constant const& c, expressions::Expression const& lowerBound, expressions::Expression const& upperBound,
                                           bool observable) const {
    STORM_LOG_THROW(this->getModelType() == ModelType::POMDP || observable, storm::exceptions::InvalidArgumentException,
                    "Variables can only be unobservable in POMDPs");
    std::vector<BooleanVariable> newBooleanVariables = globalBooleanVariables;
    std::vector<IntegerVariable> newIntegerVariables = globalIntegerVariables;
    std::vector<Constant> newConstants = constants;
    auto newEnd = std::remove(newConstants.begin(), newConstants.end(), c);
    newConstants.erase(newEnd, newConstants.end());  // Erase is necessary based on Erase-remove idiom
    // The following throw is moved here as this is cheaper.
    STORM_LOG_THROW(newConstants.size() == constants.size() - 1, exceptions::InvalidArgumentException, "Can only replace a constant if it is present.");
    if (c.getType().isBooleanType()) {
        newBooleanVariables.emplace_back(c.getExpressionVariable(), c.getExpression(), observable);
    } else {
        newIntegerVariables.emplace_back(c.getExpressionVariable(), lowerBound, upperBound, c.getExpression(), observable);
    }
    return Program(this->manager, this->getModelType(), newConstants, newBooleanVariables, newIntegerVariables, this->getFormulas(), this->getPlayers(),
                   modules, this->actionToIndexMap, this->getRewardModels(), this->getLabels(), this->getObservationLabels(),
                   this->getOptionalInitialConstruct(), this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

void Program::checkValidity(Program::ValidityCheckLevel lvl) const {
    // Start by checking the constant declarations.
    std::set<storm::expressions::Variable> all;
    std::set<storm::expressions::Variable> allGlobals;
    std::set<storm::expressions::Variable> globalVariables;
    std::set<storm::expressions::Variable> constants;
    for (auto const& constant : this->getConstants()) {
        // Check defining expressions of defined constants.
        if (constant.isDefined()) {
            std::set<storm::expressions::Variable> containedVariables = constant.getExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();

            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << constant.getFilename() << ", line " << constant.getLineNumber()
                                            << ": defining expression refers to unknown identifiers: " << boost::algorithm::join(illegalVariableNames, ",")
                                            << ".");
            }
        }

        // Record the new identifier for future checks.
        constants.insert(constant.getExpressionVariable());
        all.insert(constant.getExpressionVariable());
        allGlobals.insert(constant.getExpressionVariable());
    }

    // Now we check the variable declarations. We start with the global variables.
    std::set<storm::expressions::Variable> variables;
    for (auto const& variable : this->getGlobalBooleanVariables()) {
        if (variable.hasInitialValue()) {
            STORM_LOG_THROW(!this->hasInitialConstruct(), storm::exceptions::WrongFormatException,
                            "Error for  " << variable.getName() << " (" << variable.getFilename() << ", line " << variable.getLineNumber()
                                          << "): illegal to specify initial value if an initial construct is present.");

            // Check the initial value of the variable.
            std::set<storm::expressions::Variable> containedVariables = variable.getInitialValueExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();

            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                            << ": initial value expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                            << ".");
            }
        }

        // Record the new identifier for future checks.
        variables.insert(variable.getExpressionVariable());
        all.insert(variable.getExpressionVariable());
        allGlobals.insert(variable.getExpressionVariable());
        globalVariables.insert(variable.getExpressionVariable());
    }
    for (auto const& variable : this->getGlobalIntegerVariables()) {
        // Check that bound expressions of the range.
        if (variable.hasLowerBoundExpression()) {
            std::set<storm::expressions::Variable> containedVariables = variable.getLowerBoundExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();

            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                            << ": lower bound expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                            << ".");
            }
        }

        if (variable.hasUpperBoundExpression()) {
            std::set<storm::expressions::Variable> containedVariables = variable.getUpperBoundExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();
            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                            << ": upper bound expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                            << ".");
            }
        }

        if (variable.hasInitialValue()) {
            STORM_LOG_THROW(!this->hasInitialConstruct(), storm::exceptions::WrongFormatException,
                            "Error for  " << variable.getName() << " (" << variable.getFilename() << ", line " << variable.getLineNumber()
                                          << "): illegal to specify initial value if an initial construct is present.");

            // Check the initial value of the variable.
            std::set<storm::expressions::Variable> containedVariables = variable.getInitialValueExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();
            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                            << ": initial value expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                            << ".");
            }
        }

        // Record the new identifier for future checks.
        variables.insert(variable.getExpressionVariable());
        all.insert(variable.getExpressionVariable());
        allGlobals.insert(variable.getExpressionVariable());
        globalVariables.insert(variable.getExpressionVariable());
    }

    // Now go through the variables of the modules.
    for (auto const& module : this->getModules()) {
        for (auto const& variable : module.getBooleanVariables()) {
            if (variable.hasInitialValue()) {
                STORM_LOG_THROW(!this->hasInitialConstruct(), storm::exceptions::WrongFormatException,
                                "Error for  " << module.getName() << "." << variable.getName() << " (" << variable.getFilename() << ", line "
                                              << variable.getLineNumber() << "): illegal to specify initial value if an initial construct is present.");

                // Check the initial value of the variable.
                std::set<storm::expressions::Variable> containedVariables = variable.getInitialValueExpression().getVariables();
                std::set<storm::expressions::Variable> illegalVariables;
                std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                    std::inserter(illegalVariables, illegalVariables.begin()));
                bool isValid = illegalVariables.empty();
                if (!isValid) {
                    std::vector<std::string> illegalVariableNames;
                    for (auto const& var : illegalVariables) {
                        illegalVariableNames.push_back(var.getName());
                    }
                    STORM_LOG_THROW(
                        isValid, storm::exceptions::WrongFormatException,
                        "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                    << ": initial value expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
                }
            }

            // Record the new identifier for future checks.
            variables.insert(variable.getExpressionVariable());
            all.insert(variable.getExpressionVariable());
        }
        for (auto const& variable : module.getIntegerVariables()) {
            // Check that bound expressions of the range.
            if (variable.hasLowerBoundExpression()) {
                std::set<storm::expressions::Variable> containedVariables = variable.getLowerBoundExpression().getVariables();
                std::set<storm::expressions::Variable> illegalVariables;
                std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                    std::inserter(illegalVariables, illegalVariables.begin()));
                bool isValid = illegalVariables.empty();
                if (!isValid) {
                    std::vector<std::string> illegalVariableNames;
                    for (auto const& var : illegalVariables) {
                        illegalVariableNames.push_back(var.getName());
                    }
                    STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                    "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                                << ": lower bound expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                                << ".");
                }
            }

            if (variable.hasUpperBoundExpression()) {
                std::set<storm::expressions::Variable> containedVariables = variable.getUpperBoundExpression().getVariables();
                std::set<storm::expressions::Variable> illegalVariables;

                illegalVariables.clear();
                std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                    std::inserter(illegalVariables, illegalVariables.begin()));
                bool isValid = illegalVariables.empty();
                if (!isValid) {
                    std::vector<std::string> illegalVariableNames;
                    for (auto const& var : illegalVariables) {
                        illegalVariableNames.push_back(var.getName());
                    }
                    STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                    "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                                << ": upper bound expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",")
                                                << ".");
                }
            }

            if (variable.hasInitialValue()) {
                STORM_LOG_THROW(!this->hasInitialConstruct(), storm::exceptions::WrongFormatException,
                                "Error for  " << module.getName() << "." << variable.getName() << " (" << variable.getFilename() << ", line "
                                              << variable.getLineNumber() << "): illegal to specify initial value if an initial construct is present.");

                // Check the initial value of the variable.
                std::set<storm::expressions::Variable> containedVariables = variable.getInitialValueExpression().getVariables();
                std::set<storm::expressions::Variable> illegalVariables;
                illegalVariables.clear();
                std::set_difference(containedVariables.begin(), containedVariables.end(), constants.begin(), constants.end(),
                                    std::inserter(illegalVariables, illegalVariables.begin()));
                bool isValid = illegalVariables.empty();
                if (!isValid) {
                    std::vector<std::string> illegalVariableNames;
                    for (auto const& var : illegalVariables) {
                        illegalVariableNames.push_back(var.getName());
                    }
                    STORM_LOG_THROW(
                        isValid, storm::exceptions::WrongFormatException,
                        "Error in " << variable.getFilename() << ", line " << variable.getLineNumber()
                                    << ": initial value expression refers to unknown constants: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
                }
            }

            // Record the new identifier for future checks.
            variables.insert(variable.getExpressionVariable());
            all.insert(variable.getExpressionVariable());
        }

        for (auto const& variable : module.getClockVariables()) {
            // Record the new identifier for future checks.
            variables.insert(variable.getExpressionVariable());
            all.insert(variable.getExpressionVariable());
        }
    }

    // Create the set of valid identifiers for future checks.
    std::set<storm::expressions::Variable> variablesAndConstants;
    std::set_union(variables.begin(), variables.end(), constants.begin(), constants.end(), std::inserter(variablesAndConstants, variablesAndConstants.begin()));

    // Collect the formula placeholders and check formulas
    for (auto const& formula : this->getFormulas()) {
        std::set<storm::expressions::Variable> containedVariables = formula.getExpression().getVariables();
        bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
        STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                        "Error in " << formula.getFilename() << ", line " << formula.getLineNumber() << ": expression '" << formula.getExpression()
                                    << "'of formula '" << formula.getName() << "' refers to unknown identifiers.");
        if (formula.hasExpressionVariable()) {
            all.insert(formula.getExpressionVariable());
            variablesAndConstants.insert(formula.getExpressionVariable());
        }
    }

    // Check the commands and invariants of the modules.
    bool hasProbabilisticCommand = false;
    bool hasMarkovianCommand = false;
    bool hasLabeledMarkovianCommand = false;
    std::map<std::pair<storm::expressions::Variable, uint64_t>, std::pair<uint64_t, std::string>> writtenGlobalVariables;
    for (auto const& module : this->getModules()) {
        std::set<storm::expressions::Variable> legalVariables = globalVariables;
        for (auto const& variable : module.getBooleanVariables()) {
            legalVariables.insert(variable.getExpressionVariable());
        }
        for (auto const& variable : module.getIntegerVariables()) {
            legalVariables.insert(variable.getExpressionVariable());
        }
        for (auto const& variable : module.getClockVariables()) {
            legalVariables.insert(variable.getExpressionVariable());
        }

        if (module.hasInvariant()) {
            std::set<storm::expressions::Variable> containedVariables = module.getInvariant().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), variablesAndConstants.begin(), variablesAndConstants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();
            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << module.getFilename() << ", line " << module.getLineNumber() << ": invariant " << module.getInvariant()
                                            << " refers to unknown identifiers: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
            }
            STORM_LOG_THROW(module.getInvariant().hasBooleanType(), storm::exceptions::WrongFormatException,
                            "Error in " << module.getFilename() << ", line " << module.getLineNumber() << ": invariant " << module.getInvariant()
                                        << " must evaluate to type 'bool'.");
        }

        for (auto& command : module.getCommands()) {
            // Check the guard.
            std::set<storm::expressions::Variable> containedVariables = command.getGuardExpression().getVariables();
            std::set<storm::expressions::Variable> illegalVariables;
            std::set_difference(containedVariables.begin(), containedVariables.end(), variablesAndConstants.begin(), variablesAndConstants.end(),
                                std::inserter(illegalVariables, illegalVariables.begin()));
            bool isValid = illegalVariables.empty();
            if (!isValid) {
                std::vector<std::string> illegalVariableNames;
                for (auto const& var : illegalVariables) {
                    illegalVariableNames.push_back(var.getName());
                }
                STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                                "Error in " << command.getFilename() << ", line " << command.getLineNumber() << ": guard " << command.getGuardExpression()
                                            << " refers to unknown identifiers: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
            }
            STORM_LOG_THROW(
                command.getGuardExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                "Error in " << command.getFilename() << ", line " << command.getLineNumber() << ": expression for guard must evaluate to type 'bool'.");

            // Record which types of commands were seen.
            if (command.isMarkovian()) {
                hasMarkovianCommand = true;
            } else {
                hasProbabilisticCommand = true;
            }

            // If the command is Markovian and labeled, we throw an error or raise a warning, depending on
            // whether or not the PRISM compatibility mode was enabled.
            if (command.isMarkovian() && command.isLabeled()) {
                hasLabeledMarkovianCommand = true;
            }

            // Check all updates.
            for (auto const& update : command.getUpdates()) {
                containedVariables.clear();
                if (update.isLikelihoodInterval()) {
                    update.getLikelihoodExpressionInterval().first.gatherVariables(containedVariables);
                    update.getLikelihoodExpressionInterval().second.gatherVariables(containedVariables);
                } else {
                    update.getLikelihoodExpression().gatherVariables(containedVariables);
                }
                illegalVariables.clear();
                std::set_difference(containedVariables.begin(), containedVariables.end(), variablesAndConstants.begin(), variablesAndConstants.end(),
                                    std::inserter(illegalVariables, illegalVariables.begin()));
                isValid = illegalVariables.empty();
                if (!isValid) {
                    std::vector<std::string> illegalVariableNames;
                    for (auto const& var : illegalVariables) {
                        illegalVariableNames.push_back(var.getName());
                    }
                    STORM_LOG_THROW(
                        isValid, storm::exceptions::WrongFormatException,
                        "Error in " << command.getFilename() << ", line " << command.getLineNumber()
                                    << ": likelihood expression refers to unknown identifiers: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
                }

                // Check all assignments.
                std::set<storm::expressions::Variable> alreadyAssignedVariables;
                for (auto const& assignment : update.getAssignments()) {
                    storm::expressions::Variable assignedVariable = manager->getVariable(assignment.getVariableName());

                    if (legalVariables.find(assignedVariable) == legalVariables.end()) {
                        if (all.find(assignedVariable) != all.end()) {
                            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                            "Error in " << command.getFilename() << ", line " << command.getLineNumber()
                                                        << ": assignment illegally refers to variable '" << assignment.getVariableName() << "'.");
                        } else {
                            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                            "Error in " << command.getFilename() << ", line " << command.getLineNumber()
                                                        << ": assignment refers to unknown variable '" << assignment.getVariableName() << "'.");
                        }
                    }
                    STORM_LOG_THROW(alreadyAssignedVariables.find(assignedVariable) == alreadyAssignedVariables.end(), storm::exceptions::WrongFormatException,
                                    "Error in " << command.getFilename() << ", line " << command.getLineNumber() << ": duplicate assignment to variable '"
                                                << assignment.getVariableName() << "'.");
                    STORM_LOG_THROW(assignedVariable.getType() == assignment.getExpression().getType() ||
                                        (assignedVariable.getType().isRationalType() && assignment.getExpression().getType().isNumericalType()),
                                    storm::exceptions::WrongFormatException,
                                    "Error in " << command.getFilename() << ", line " << command.getLineNumber() << ": illegally assigning a value of type '"
                                                << assignment.getExpression().getType() << "' to variable '" << assignment.getVariableName() << "' of type '"
                                                << assignedVariable.getType() << "'.");

                    if (command.isLabeled() && globalVariables.find(assignedVariable) != globalVariables.end()) {
                        std::pair<storm::expressions::Variable, uint64_t> variableActionIndexPair(assignedVariable, command.getActionIndex());
                        std::pair<uint64_t, std::string> lineModuleNamePair(command.getLineNumber(), module.getName());
                        auto insertionResult = writtenGlobalVariables.emplace(variableActionIndexPair, lineModuleNamePair);
                        STORM_LOG_THROW(
                            insertionResult.second || insertionResult.first->second.second == module.getName(), storm::exceptions::WrongFormatException,
                            "Error in " << command.getFilename() << ", line " << command.getLineNumber() << ": Syncronizing command with action label '"
                                        << command.getActionName() << "' illegally assigns a value to global variable '" << assignedVariable.getName()
                                        << "'. Previous assignment to the variable at line " << insertionResult.first->second.first << " in module '"
                                        << insertionResult.first->second.second << "'.");
                    }

                    containedVariables = assignment.getExpression().getVariables();
                    illegalVariables.clear();
                    std::set_difference(containedVariables.begin(), containedVariables.end(), variablesAndConstants.begin(), variablesAndConstants.end(),
                                        std::inserter(illegalVariables, illegalVariables.begin()));
                    isValid = illegalVariables.empty();
                    if (!isValid) {
                        std::vector<std::string> illegalVariableNames;
                        for (auto const& var : illegalVariables) {
                            illegalVariableNames.push_back(var.getName());
                        }
                        STORM_LOG_THROW(
                            isValid, storm::exceptions::WrongFormatException,
                            "Error in " << command.getFilename() << ", line " << command.getLineNumber()
                                        << ": assigned expression refers to unknown identifiers: " << boost::algorithm::join(illegalVariableNames, ",") << ".");
                    }

                    // Add the current variable to the set of assigned variables (of this update).
                    alreadyAssignedVariables.insert(assignedVariable);
                }
            }
        }
    }

    if (hasLabeledMarkovianCommand) {
        if (prismCompatibility) {
            STORM_LOG_WARN_COND(
                false, "The model uses synchronizing Markovian commands. This may lead to unexpected verification results, because of unclear semantics.");
        } else {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "The model uses synchronizing Markovian commands. This may lead to unexpected verification results, because of unclear semantics.");
        }
    }

    if (this->getModelType() == Program::ModelType::DTMC || this->getModelType() == Program::ModelType::MDP) {
        STORM_LOG_THROW(!hasMarkovianCommand, storm::exceptions::WrongFormatException, "Discrete-time model must not have Markovian commands.");
    } else if (this->getModelType() == Program::ModelType::CTMC) {
        STORM_LOG_THROW(!hasProbabilisticCommand, storm::exceptions::WrongFormatException,
                        "The input model is a CTMC, but uses probabilistic commands like they are used in PRISM. Please use Markovian commands instead or turn "
                        "on the PRISM compatibility mode using the flag '-pc'.");
    }

    // Now check the reward models.
    for (auto const& rewardModel : this->getRewardModels()) {
        for (auto const& stateReward : rewardModel.getStateRewards()) {
            std::set<storm::expressions::Variable> containedVariables = stateReward.getStatePredicateExpression().getVariables();
            bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << stateReward.getFilename() << ", line " << stateReward.getLineNumber()
                                        << ": state reward expression refers to unknown identifiers.");
            STORM_LOG_THROW(
                stateReward.getStatePredicateExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                "Error in " << stateReward.getFilename() << ", line " << stateReward.getLineNumber() << ": state predicate must evaluate to type 'bool'.");

            containedVariables = stateReward.getRewardValueExpression().getVariables();
            isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << stateReward.getFilename() << ", line " << stateReward.getLineNumber()
                                        << ": state reward value expression refers to unknown identifiers.");
            STORM_LOG_THROW(stateReward.getRewardValueExpression().hasNumericalType(), storm::exceptions::WrongFormatException,
                            "Error in " << stateReward.getFilename() << ", line " << stateReward.getLineNumber()
                                        << ": reward value expression must evaluate to numerical type.");
        }

        for (auto const& stateActionReward : rewardModel.getStateActionRewards()) {
            std::set<storm::expressions::Variable> containedVariables = stateActionReward.getStatePredicateExpression().getVariables();
            bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << stateActionReward.getFilename() << ", line " << stateActionReward.getLineNumber()
                                        << ": state reward expression refers to unknown identifiers.");
            STORM_LOG_THROW(stateActionReward.getStatePredicateExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                            "Error in " << stateActionReward.getFilename() << ", line " << stateActionReward.getLineNumber()
                                        << ": state predicate must evaluate to type 'bool'.");

            containedVariables = stateActionReward.getRewardValueExpression().getVariables();
            isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << stateActionReward.getFilename() << ", line " << stateActionReward.getLineNumber()
                                        << ": state reward value expression refers to unknown identifiers.");
            STORM_LOG_THROW(stateActionReward.getRewardValueExpression().hasNumericalType(), storm::exceptions::WrongFormatException,
                            "Error in " << stateActionReward.getFilename() << ", line " << stateActionReward.getLineNumber()
                                        << ": reward value expression must evaluate to numerical type.");
        }

        for (auto const& transitionReward : rewardModel.getTransitionRewards()) {
            std::set<storm::expressions::Variable> containedVariables = transitionReward.getSourceStatePredicateExpression().getVariables();
            bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": state reward expression refers to unknown identifiers.");
            STORM_LOG_THROW(transitionReward.getSourceStatePredicateExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": state predicate must evaluate to type 'bool'.");

            containedVariables = transitionReward.getTargetStatePredicateExpression().getVariables();
            isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": state reward expression refers to unknown identifiers.");
            STORM_LOG_THROW(transitionReward.getTargetStatePredicateExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": state predicate must evaluate to type 'bool'.");

            containedVariables = transitionReward.getRewardValueExpression().getVariables();
            isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
            STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": state reward value expression refers to unknown identifiers.");
            STORM_LOG_THROW(transitionReward.getRewardValueExpression().hasNumericalType(), storm::exceptions::WrongFormatException,
                            "Error in " << transitionReward.getFilename() << ", line " << transitionReward.getLineNumber()
                                        << ": reward value expression must evaluate to numerical type.");
        }
    }

    // Check the initial states expression.
    if (this->hasInitialConstruct()) {
        std::set<storm::expressions::Variable> containedIdentifiers = this->getInitialConstruct().getInitialStatesExpression().getVariables();
        bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedIdentifiers.begin(), containedIdentifiers.end());
        STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                        "Error in " << this->getInitialConstruct().getFilename() << ", line " << this->getInitialConstruct().getLineNumber()
                                    << ": initial construct refers to unknown identifiers.");
    }

    // Check the system composition if given.
    if (systemCompositionConstruct) {
        CompositionValidityChecker checker(*this);
        checker.check(systemCompositionConstruct.get().getSystemComposition());
    }

    // Check the labels.
    for (auto const& label : this->getLabels()) {
        std::set<storm::expressions::Variable> containedVariables = label.getStatePredicateExpression().getVariables();
        bool isValid = std::includes(variablesAndConstants.begin(), variablesAndConstants.end(), containedVariables.begin(), containedVariables.end());
        STORM_LOG_THROW(isValid, storm::exceptions::WrongFormatException,
                        "Error in " << label.getFilename() << ", line " << label.getLineNumber() << ": label expression refers to unknown identifiers.");
        STORM_LOG_THROW(label.getStatePredicateExpression().hasBooleanType(), storm::exceptions::WrongFormatException,
                        "Error in " << label.getFilename() << ", line " << label.getLineNumber() << ": label predicate must evaluate to type 'bool'.");
    }

    // Check the players
    for (auto const& player : this->getPlayers()) {
        // The stored action/module names shall be available
        for (auto const& controlledAction : player.getActions()) {
            STORM_LOG_THROW(this->hasAction(controlledAction), storm::exceptions::InternalException,
                            "Error in " << player.getFilename() << ", line " << player.getLineNumber() << ": The player controlled action " << controlledAction
                                        << " is not available.");
        }
        for (auto const& controlledModule : player.getModules()) {
            STORM_LOG_THROW(this->hasModule(controlledModule), storm::exceptions::InternalException,
                            "Error in " << player.getFilename() << ", line " << player.getLineNumber() << ": The player controlled module " << controlledModule
                                        << " is not available.");
        }
    }

    if (lvl >= Program::ValidityCheckLevel::READYFORPROCESSING) {
        // We check for each global variable and each labeled command, whether there is at most one instance writing to that variable.
        std::set<std::pair<std::string, std::string>> globalBVarsWrittenToByCommand;
        std::set<std::pair<std::string, std::string>> globalIVarsWrittenToByCommand;
        for (auto const& module : this->getModules()) {
            std::set<std::pair<std::string, std::string>> globalBVarsWrittenToByCommandInThisModule;
            std::set<std::pair<std::string, std::string>> globalIVarsWrittenToByCommandInThisModule;
            for (auto const& command : module.getCommands()) {
                if (!command.isLabeled())
                    continue;
                for (auto const& update : command.getUpdates()) {
                    for (auto const& assignment : update.getAssignments()) {
                        if (this->globalBooleanVariableExists(assignment.getVariable().getName())) {
                            globalBVarsWrittenToByCommandInThisModule.insert({assignment.getVariable().getName(), command.getActionName()});
                        } else if (this->globalIntegerVariableExists(assignment.getVariable().getName())) {
                            globalIVarsWrittenToByCommandInThisModule.insert({assignment.getVariable().getName(), command.getActionName()});
                        }
                    }
                }
            }
            for (auto const& entry : globalIVarsWrittenToByCommandInThisModule) {
                STORM_LOG_THROW(globalIVarsWrittenToByCommand.find(entry) == globalIVarsWrittenToByCommand.end(), storm::exceptions::WrongFormatException,
                                "Error in " << module.getFilename() << ", line " << module.getLineNumber()
                                            << ": assignment of (possibly) synchronizing command with label '" << entry.second
                                            << "' writes to global variable '" << entry.first << "'.");
            }
            for (auto const& entry : globalBVarsWrittenToByCommandInThisModule) {
                STORM_LOG_THROW(globalBVarsWrittenToByCommand.find(entry) == globalBVarsWrittenToByCommand.end(), storm::exceptions::WrongFormatException,
                                "Error in " << module.getFilename() << ", line " << module.getLineNumber()
                                            << ": assignment of (possibly) synchronizing command with label '" << entry.second
                                            << "' writes to global variable '" << entry.first << "'.");
            }
        }
    }
}

Program Program::simplify() {
    // Start by substituting the constants, because this will potentially erase some commands or even actions.
    Program substitutedProgram = this->substituteConstantsFormulas();

    // As we possibly delete some commands and some actions might be dropped from modules altogether, we need to
    // maintain a list of actions that we need to remove in other modules. For example, if module A loses all [a]
    // commands, we need to delete all [a] commands from all other modules as well. If we do not do that, we will
    // remove the forced synchronization that was there before.
    std::set<uint_fast64_t> actionIndicesToDelete;

    std::vector<Module> newModules;
    std::vector<Constant> newConstants = substitutedProgram.getConstants();
    for (auto const& module : substitutedProgram.getModules()) {
        // Discard all commands with a guard equivalent to false and remove identity assignments from the updates.
        std::vector<Command> newCommands;
        for (auto const& command : module.getCommands()) {
            if (!command.getGuardExpression().isFalse()) {
                newCommands.emplace_back(command.simplify());
            }
        }

        // Substitute variables by global constants if possible.
        std::map<storm::expressions::Variable, storm::expressions::Expression> booleanVars;
        std::map<storm::expressions::Variable, storm::expressions::Expression> integerVars;
        for (auto const& variable : module.getBooleanVariables()) {
            booleanVars.emplace(variable.getExpressionVariable(), variable.getInitialValueExpression());
        }
        for (auto const& variable : module.getIntegerVariables()) {
            integerVars.emplace(variable.getExpressionVariable(), variable.getInitialValueExpression());
        }

        // Collect all variables that are being written. These variables cannot be turned to constants.
        for (auto const& command : newCommands) {
            // Check all updates.
            for (auto const& update : command.getUpdates()) {
                // Check all assignments.
                for (auto const& assignment : update.getAssignments()) {
                    if (assignment.getVariable().getType().isBooleanType()) {
                        auto it = booleanVars.find(assignment.getVariable());
                        if (it != booleanVars.end()) {
                            booleanVars.erase(it);
                        }
                    } else {
                        auto it = integerVars.find(assignment.getVariable());
                        if (it != integerVars.end()) {
                            integerVars.erase(it);
                        }
                    }
                }
            }
        }

        std::vector<storm::prism::BooleanVariable> newBooleanVars;
        for (auto const& variable : module.getBooleanVariables()) {
            if (booleanVars.find(variable.getExpressionVariable()) == booleanVars.end()) {
                newBooleanVars.push_back(variable);
            }
        }
        std::vector<storm::prism::IntegerVariable> newIntegerVars;
        for (auto const& variable : module.getIntegerVariables()) {
            if (integerVars.find(variable.getExpressionVariable()) == integerVars.end()) {
                newIntegerVars.push_back(variable);
            }
        }

        for (auto const& variable : module.getBooleanVariables()) {
            if (booleanVars.find(variable.getExpressionVariable()) != booleanVars.end()) {
                if (variable.hasInitialValue()) {
                    newConstants.emplace_back(variable.getExpressionVariable(), variable.getInitialValueExpression());
                } else {
                    newBooleanVars.push_back(variable);
                }
            }
        }
        for (auto const& variable : module.getIntegerVariables()) {
            if (integerVars.find(variable.getExpressionVariable()) != integerVars.end()) {
                if (variable.hasInitialValue()) {
                    newConstants.emplace_back(variable.getExpressionVariable(), variable.getInitialValueExpression());
                } else {
                    newIntegerVars.push_back(variable);
                }
            }
        }

        // we currently do not simplify clock variables or invariants
        newModules.emplace_back(module.getName(), newBooleanVars, newIntegerVars, module.getClockVariables(), module.getInvariant(), newCommands);

        // Determine the set of action indices that have been deleted entirely.
        std::set_difference(module.getSynchronizingActionIndices().begin(), module.getSynchronizingActionIndices().end(),
                            newModules.back().getSynchronizingActionIndices().begin(), newModules.back().getSynchronizingActionIndices().end(),
                            std::inserter(actionIndicesToDelete, actionIndicesToDelete.begin()));
    }

    // If we have to delete whole actions, do so now.
    std::map<std::string, uint_fast64_t> newActionToIndexMap;
    std::vector<RewardModel> newRewardModels;
    std::vector<Player> newPlayers;
    if (!actionIndicesToDelete.empty()) {
        storm::storage::FlatSet<uint_fast64_t> actionsToKeep;
        std::set_difference(this->getSynchronizingActionIndices().begin(), this->getSynchronizingActionIndices().end(), actionIndicesToDelete.begin(),
                            actionIndicesToDelete.end(), std::inserter(actionsToKeep, actionsToKeep.begin()));

        // Insert the silent action as this is not contained in the synchronizing action indices.
        actionsToKeep.insert(0);

        std::vector<Module> cleanedModules;
        cleanedModules.reserve(newModules.size());
        for (auto const& module : newModules) {
            cleanedModules.emplace_back(module.restrictActionIndices(actionsToKeep));
        }
        newModules = std::move(cleanedModules);

        newRewardModels.reserve(substitutedProgram.getNumberOfRewardModels());
        for (auto const& rewardModel : substitutedProgram.getRewardModels()) {
            newRewardModels.emplace_back(rewardModel.restrictActionRelatedRewards(actionsToKeep));
        }

        // Restrict action name to index mapping. Old action indices remain valid.
        for (auto const& entry : this->getActionNameToIndexMapping()) {
            if (actionsToKeep.find(entry.second) != actionsToKeep.end()) {
                newActionToIndexMap.emplace(entry.first, entry.second);
            }
        }

        // Restrict player controlled actions
        for (auto const& player : this->getPlayers()) {
            std::unordered_set<std::string> newControlledActions;
            for (auto const& act : player.getActions()) {
                if (newActionToIndexMap.count(act) != 0) {
                    newControlledActions.insert(act);
                }
            }
            newPlayers.emplace_back(player.getName(), player.getModules(), newControlledActions, player.getFilename(), player.getLineNumber());
        }
    }

    std::vector<Label> newLabels;
    for (auto const& label : this->getLabels()) {
        newLabels.emplace_back(label.getName(), label.getStatePredicateExpression().simplify());
    }

    return Program(this->manager, modelType, newConstants, getGlobalBooleanVariables(), getGlobalIntegerVariables(), getFormulas(),
                   actionIndicesToDelete.empty() ? this->getPlayers() : newPlayers, newModules,
                   actionIndicesToDelete.empty() ? getActionNameToIndexMapping() : newActionToIndexMap,
                   actionIndicesToDelete.empty() ? this->getRewardModels() : newRewardModels, newLabels, getObservationLabels(), getOptionalInitialConstruct(),
                   this->getOptionalSystemCompositionConstruct(), prismCompatibility);
}

Program Program::flattenModules(std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory) const {
    // If the current program has only one module, we can simply return a copy.
    if (this->getNumberOfModules() == 1) {
        return Program(*this);
    }

    STORM_LOG_THROW(this->getModelType() == ModelType::DTMC || this->getModelType() == ModelType::MDP, storm::exceptions::InvalidTypeException,
                    "Unable to flatten modules for model of type '" << this->getModelType() << "'.");

    // Otherwise, we need to actually flatten the contained modules.

    // Get an SMT solver for computing the possible guard combinations.
    std::unique_ptr<storm::solver::SmtSolver> solver = smtSolverFactory->create(*manager);

    // Set up the data we need to gather to create the flat module.
    std::stringstream newModuleName;
    std::vector<storm::prism::BooleanVariable> allBooleanVariables;
    std::vector<storm::prism::IntegerVariable> allIntegerVariables;
    std::vector<storm::prism::ClockVariable> allClockVariables;
    std::vector<storm::prism::Command> newCommands;
    uint_fast64_t nextCommandIndex = 0;
    uint_fast64_t nextUpdateIndex = 0;

    // Assert the values of the constants.
    for (auto const& constant : this->getConstants()) {
        if (constant.isDefined()) {
            if (constant.getType().isBooleanType()) {
                solver->add(storm::expressions::iff(constant.getExpressionVariable(), constant.getExpression()));
            } else {
                solver->add(constant.getExpressionVariable() == constant.getExpression());
            }
        }
    }

    // Assert the bounds of the global variables.
    for (auto const& variable : this->getGlobalIntegerVariables()) {
        solver->add(variable.getRangeExpression());
    }

    // Make the global variables local, such that the resulting module covers all occurring variables. Note that
    // this is just for simplicity and is not needed.
    allBooleanVariables.insert(allBooleanVariables.end(), this->getGlobalBooleanVariables().begin(), this->getGlobalBooleanVariables().end());
    allIntegerVariables.insert(allIntegerVariables.end(), this->getGlobalIntegerVariables().begin(), this->getGlobalIntegerVariables().end());
    storm::expressions::Expression newInvariant;

    // Now go through the modules, gather the variables, construct the name of the new module and assert the
    // bounds of the discovered variables.
    for (auto const& module : this->getModules()) {
        newModuleName << module.getName() << "_";
        allBooleanVariables.insert(allBooleanVariables.end(), module.getBooleanVariables().begin(), module.getBooleanVariables().end());
        allIntegerVariables.insert(allIntegerVariables.end(), module.getIntegerVariables().begin(), module.getIntegerVariables().end());
        allClockVariables.insert(allClockVariables.end(), module.getClockVariables().begin(), module.getClockVariables().end());

        for (auto const& variable : module.getIntegerVariables()) {
            solver->add(variable.getRangeExpression());
        }

        if (module.hasInvariant()) {
            newInvariant = newInvariant.isInitialized() ? (newInvariant && module.getInvariant()) : module.getInvariant();
        }

        // The commands without a synchronizing action name, can simply be copied (plus adjusting the global
        // indices of the command and its updates).
        for (auto const& command : module.getCommands()) {
            if (!command.isLabeled()) {
                std::vector<storm::prism::Update> updates;
                updates.reserve(command.getUpdates().size());

                for (auto const& update : command.getUpdates()) {
                    updates.push_back(
                        storm::prism::Update(nextUpdateIndex, update.getLikelihoodExpression(), update.getAssignments(), update.getFilename(), 0));
                    ++nextUpdateIndex;
                }

                newCommands.push_back(storm::prism::Command(nextCommandIndex, command.isMarkovian(), actionToIndexMap.find("")->second, "",
                                                            command.getGuardExpression(), updates, command.getFilename(), 0));
                ++nextCommandIndex;
            }
        }
    }

    // Save state of solver so that we can always restore the point where we have exactly the constant values
    // and variables bounds on the assertion stack.
    solver->push();

    // Now we need to enumerate all possible combinations of synchronizing commands. For this, we iterate over
    // all actions and let the solver enumerate the possible combinations of commands that can be enabled together.
    for (auto const& actionIndex : this->getSynchronizingActionIndices()) {
        bool noCombinationsForAction = false;

        // Prepare the list that stores for each module the list of commands with the given action.
        std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>> possibleCommands;

        for (auto const& module : this->getModules()) {
            // If the module has no command with this action, we can skip it.
            if (!module.hasActionIndex(actionIndex)) {
                continue;
            }

            std::set<uint_fast64_t> const& commandIndices = module.getCommandIndicesByActionIndex(actionIndex);

            // If there is no command even though the module has this action, there is no valid command
            // combination with this action.
            if (commandIndices.empty()) {
                noCombinationsForAction = true;
                break;
            }

            // Prepare empty list of commands for this module.
            possibleCommands.push_back(std::vector<std::reference_wrapper<storm::prism::Command const>>());

            // Add references to the commands labeled with the current action.
            for (auto const& commandIndex : commandIndices) {
                possibleCommands.back().push_back(module.getCommand(commandIndex));
            }
        }

        // If there are no valid combinations for the action, we need to skip the generation of synchronizing
        // commands.
        if (!noCombinationsForAction) {
            // Save the solver state to be able to restore it when this action index is done.
            solver->push();

            // Start by creating a fresh auxiliary variable for each command and link it with the guard.
            std::vector<std::vector<storm::expressions::Variable>> commandVariables(possibleCommands.size());
            std::vector<storm::expressions::Variable> allCommandVariables;
            for (uint_fast64_t outerIndex = 0; outerIndex < possibleCommands.size(); ++outerIndex) {
                // Create auxiliary variables and link them with the guards.
                for (uint_fast64_t innerIndex = 0; innerIndex < possibleCommands[outerIndex].size(); ++innerIndex) {
                    commandVariables[outerIndex].push_back(manager->declareFreshBooleanVariable());
                    allCommandVariables.push_back(commandVariables[outerIndex].back());
                    solver->add(implies(commandVariables[outerIndex].back(), possibleCommands[outerIndex][innerIndex].get().getGuardExpression()));
                }

                storm::expressions::Expression atLeastOneCommandFromModule = manager->boolean(false);
                for (auto const& commandVariable : commandVariables[outerIndex]) {
                    atLeastOneCommandFromModule = atLeastOneCommandFromModule || commandVariable;
                }
                solver->add(atLeastOneCommandFromModule);
            }

            // Now we are in a position to start the enumeration over all command variables. While doing so, we
            // keep track of previously seen command combinations, because the AllSat procedures are not
            // always guaranteed to only provide distinct models.
            std::unordered_set<std::vector<uint_fast64_t>, storm::utility::vector::VectorHash<uint_fast64_t>> seenCommandCombinations;
            solver->allSat(allCommandVariables, [&](storm::solver::SmtSolver::ModelReference& modelReference) -> bool {
                // Now we need to reconstruct the chosen commands from the valuation of the command variables.
                std::vector<std::vector<std::reference_wrapper<Command const>>> chosenCommands(possibleCommands.size());

                for (uint_fast64_t outerIndex = 0; outerIndex < commandVariables.size(); ++outerIndex) {
                    for (uint_fast64_t innerIndex = 0; innerIndex < commandVariables[outerIndex].size(); ++innerIndex) {
                        if (modelReference.getBooleanValue(commandVariables[outerIndex][innerIndex])) {
                            chosenCommands[outerIndex].push_back(possibleCommands[outerIndex][innerIndex]);
                        }
                    }
                }

                // Now that we have retrieved the commands, we need to build their synchronizations and add them
                // to the flattened module.
                std::vector<std::vector<std::reference_wrapper<Command const>>::const_iterator> iterators;
                for (auto const& element : chosenCommands) {
                    iterators.push_back(element.begin());
                }

                bool movedAtLeastOneIterator = false;
                std::vector<std::reference_wrapper<Command const>> commandCombination(chosenCommands.size(), chosenCommands.front().front());
                std::vector<uint_fast64_t> commandCombinationIndices(iterators.size());
                do {
                    for (uint_fast64_t index = 0; index < iterators.size(); ++index) {
                        commandCombination[index] = *iterators[index];
                        commandCombinationIndices[index] = commandCombination[index].get().getGlobalIndex();
                    }

                    // Only add the command combination if it was not previously seen.
                    auto seenIt = seenCommandCombinations.find(commandCombinationIndices);
                    if (seenIt == seenCommandCombinations.end()) {
                        newCommands.push_back(synchronizeCommands(nextCommandIndex, actionIndex, nextUpdateIndex, indexToActionMap.find(actionIndex)->second,
                                                                  commandCombination));
                        seenCommandCombinations.insert(commandCombinationIndices);

                        // Move the counters appropriately.
                        ++nextCommandIndex;
                        nextUpdateIndex += newCommands.back().getNumberOfUpdates();
                    }

                    movedAtLeastOneIterator = false;
                    for (uint_fast64_t index = 0; index < iterators.size(); ++index) {
                        ++iterators[index];
                        if (iterators[index] != chosenCommands[index].cend()) {
                            movedAtLeastOneIterator = true;
                            break;
                        } else {
                            iterators[index] = chosenCommands[index].cbegin();
                        }
                    }
                } while (movedAtLeastOneIterator);

                return true;
            });

            solver->pop();
        }
    }

    // Finally, we can create the module and the program and return it.
    storm::prism::Module singleModule(newModuleName.str(), allBooleanVariables, allIntegerVariables, allClockVariables, newInvariant, newCommands,
                                      this->getFilename(), 0);

    return Program(manager, this->getModelType(), this->getConstants(), std::vector<storm::prism::BooleanVariable>(),
                   std::vector<storm::prism::IntegerVariable>(), this->getFormulas(), this->getPlayers(), {singleModule}, actionToIndexMap,
                   this->getRewardModels(), this->getLabels(), this->getObservationLabels(), this->getOptionalInitialConstruct(),
                   this->getOptionalSystemCompositionConstruct(), prismCompatibility, this->getFilename(), 0, true);
}

std::vector<Constant> Program::usedConstants() const {
    std::unordered_set<expressions::Variable> vars;
    for (auto const& m : this->modules) {
        for (auto const& c : m.getCommands()) {
            auto const& found_gex = c.getGuardExpression().getVariables();
            vars.insert(found_gex.begin(), found_gex.end());
            for (auto const& u : c.getUpdates()) {
                auto const& found_lex = u.getLikelihoodExpression().getVariables();
                vars.insert(found_lex.begin(), found_lex.end());
                for (auto const& a : u.getAssignments()) {
                    auto const& found_ass = a.getExpression().getVariables();
                    vars.insert(found_ass.begin(), found_ass.end());
                }
            }
        }
        for (auto const& v : m.getBooleanVariables()) {
            if (v.hasInitialValue()) {
                auto const& found_def = v.getInitialValueExpression().getVariables();
                vars.insert(found_def.begin(), found_def.end());
            }
        }
        for (auto const& v : m.getIntegerVariables()) {
            if (v.hasInitialValue()) {
                auto const& found_def = v.getInitialValueExpression().getVariables();
                vars.insert(found_def.begin(), found_def.end());
            }
        }
    }

    for (auto const& f : this->formulas) {
        auto const& found_def = f.getExpression().getVariables();
        vars.insert(found_def.begin(), found_def.end());
    }

    for (auto const& v : this->constants) {
        if (v.isDefined()) {
            auto const& found_def = v.getExpression().getVariables();
            vars.insert(found_def.begin(), found_def.end());
        }
    }

    for (auto const& v : this->globalBooleanVariables) {
        if (v.hasInitialValue()) {
            auto const& found_def = v.getExpression().getVariables();
            vars.insert(found_def.begin(), found_def.end());
        }
    }

    for (auto const& v : this->globalIntegerVariables) {
        if (v.hasInitialValue()) {
            auto const& found_def = v.getExpression().getVariables();
            vars.insert(found_def.begin(), found_def.end());
        }
    }

    std::unordered_set<uint64_t> varIndices;
    for (auto const& v : vars) {
        varIndices.insert(v.getIndex());
    }

    std::vector<Constant> usedConstants;
    for (auto const& c : this->constants) {
        if (varIndices.count(c.getExpressionVariable().getIndex())) {
            usedConstants.push_back(c);
        }
    }

    return usedConstants;
}

std::unordered_map<uint_fast64_t, std::string> Program::buildCommandIndexToActionNameMap() const {
    std::unordered_map<uint_fast64_t, std::string> res;
    for (auto const& m : this->modules) {
        for (auto const& c : m.getCommands()) {
            res.emplace(c.getGlobalIndex(), c.getActionName());
        }
    }
    return res;
}

std::unordered_map<uint_fast64_t, std::string> Program::buildActionIndexToActionNameMap() const {
    std::unordered_map<uint_fast64_t, std::string> res;
    for (auto const& nameIndexPair : actionToIndexMap) {
        res.emplace(nameIndexPair.second, nameIndexPair.first);
    }
    return res;
}

std::unordered_map<uint_fast64_t, uint_fast64_t> Program::buildCommandIndexToActionIndex() const {
    std::unordered_map<uint_fast64_t, uint_fast64_t> res;
    for (auto const& m : this->modules) {
        for (auto const& c : m.getCommands()) {
            res.emplace(c.getGlobalIndex(), c.getActionIndex());
        }
    }
    return res;
}

Command Program::synchronizeCommands(uint_fast64_t newCommandIndex, uint_fast64_t actionIndex, uint_fast64_t firstUpdateIndex, std::string const& actionName,
                                     std::vector<std::reference_wrapper<Command const>> const& commands) const {
    // To construct the synchronous product of the commands, we need to store a list of its updates.
    std::vector<storm::prism::Update> newUpdates;
    uint_fast64_t numberOfUpdates = 1;
    for (uint_fast64_t i = 0; i < commands.size(); ++i) {
        numberOfUpdates *= commands[i].get().getNumberOfUpdates();
    }
    newUpdates.reserve(numberOfUpdates);

    // Initialize all update iterators.
    std::vector<std::vector<storm::prism::Update>::const_iterator> updateIterators;
    for (uint_fast64_t i = 0; i < commands.size(); ++i) {
        updateIterators.push_back(commands[i].get().getUpdates().cbegin());
    }

    bool doneUpdates = false;
    do {
        // We create the new likelihood expression by multiplying the particapting updates' expressions.
        storm::expressions::Expression newLikelihoodExpression = updateIterators[0]->getLikelihoodExpression();
        for (uint_fast64_t i = 1; i < updateIterators.size(); ++i) {
            newLikelihoodExpression = newLikelihoodExpression * updateIterators[i]->getLikelihoodExpression();
        }

        // Now concatenate all assignments of all participating updates.
        std::vector<storm::prism::Assignment> newAssignments;
        for (uint_fast64_t i = 0; i < updateIterators.size(); ++i) {
            newAssignments.insert(newAssignments.end(), updateIterators[i]->getAssignments().begin(), updateIterators[i]->getAssignments().end());
        }

        // Then we are ready to create the new update.
        newUpdates.push_back(storm::prism::Update(firstUpdateIndex, newLikelihoodExpression, newAssignments, this->getFilename(), 0));
        ++firstUpdateIndex;

        // Now check whether there is some update combination we have not yet explored.
        bool movedIterator = false;
        for (int_fast64_t j = updateIterators.size() - 1; j >= 0; --j) {
            ++updateIterators[j];
            if (updateIterators[j] != commands[j].get().getUpdates().cend()) {
                movedIterator = true;
                break;
            } else {
                // Reset the iterator to the beginning of the list.
                updateIterators[j] = commands[j].get().getUpdates().cbegin();
            }
        }

        doneUpdates = !movedIterator;
    } while (!doneUpdates);

    storm::expressions::Expression newGuard = commands[0].get().getGuardExpression();
    for (uint_fast64_t i = 1; i < commands.size(); ++i) {
        newGuard = newGuard && commands[i].get().getGuardExpression();
    }

    return Command(newCommandIndex, false, actionIndex, actionName, newGuard, newUpdates, this->getFilename(), 0);
}

storm::jani::Model Program::toJani(bool allVariablesGlobal, std::string suffix) const {
    ToJaniConverter converter;
    auto janiModel = converter.convert(*this, allVariablesGlobal, {}, suffix);
    STORM_LOG_WARN_COND(!converter.labelsWereRenamed(), "Labels were renamed in PRISM-to-JANI conversion, but the mapping is not stored.");
    STORM_LOG_WARN_COND(!converter.rewardModelsWereRenamed(), "Rewardmodels were renamed in PRISM-to-JANI conversion, but the mapping is not stored.");
    return janiModel;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> Program::toJani(std::vector<storm::jani::Property> const& properties, bool allVariablesGlobal,
                                                                                  std::string suffix) const {
    ToJaniConverter converter;
    std::set<storm::expressions::Variable> variablesToMakeGlobal;
    if (!allVariablesGlobal) {
        for (auto const& prop : properties) {
            auto vars = prop.getUsedVariablesAndConstants();
            variablesToMakeGlobal.insert(vars.begin(), vars.end());
        }
    }
    auto janiModel = converter.convert(*this, allVariablesGlobal, variablesToMakeGlobal, suffix);
    std::vector<storm::jani::Property> newProperties;
    if (converter.labelsWereRenamed() || converter.rewardModelsWereRenamed()) {
        newProperties = converter.applyRenaming(properties);
    } else {
        newProperties = properties;  // Nothing to be done here. Notice that the copy operation is suboptimal.
    }
    return std::make_pair(janiModel, newProperties);
}

uint64_t Program::getHighestCommandIndex() const {
    uint64_t highest = 0;
    for (auto const& m : getModules()) {
        for (auto const& c : m.getCommands()) {
            highest = std::max(highest, c.getGlobalIndex());
        }
    }
    return highest;
}

storm::expressions::ExpressionManager& Program::getManager() const {
    return *this->manager;
}

void Program::createMissingInitialValues() {
    for (auto& variable : globalBooleanVariables) {
        variable.createMissingInitialValue();
    }
    for (auto& variable : globalIntegerVariables) {
        variable.createMissingInitialValue();
    }
}

std::ostream& operator<<(std::ostream& out, Program::ModelType const& type) {
    switch (type) {
        case Program::ModelType::UNDEFINED:
            out << "undefined";
            break;
        case Program::ModelType::DTMC:
            out << "dtmc";
            break;
        case Program::ModelType::CTMC:
            out << "ctmc";
            break;
        case Program::ModelType::MDP:
            out << "mdp";
            break;
        case Program::ModelType::CTMDP:
            out << "ctmdp";
            break;
        case Program::ModelType::MA:
            out << "ma";
            break;
        case Program::ModelType::POMDP:
            out << "pomdp";
            break;
        case Program::ModelType::PTA:
            out << "pta";
            break;
        case Program::ModelType::SMG:
            out << "smg";
            break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& stream, Program const& program) {
    stream << program.getModelType() << '\n';
    for (auto const& constant : program.getConstants()) {
        stream << constant << '\n';
    }
    stream << '\n';

    for (auto const& player : program.getPlayers()) {
        stream << player << '\n';
    }

    for (auto const& variable : program.getGlobalBooleanVariables()) {
        stream << "global " << variable << '\n';
    }
    for (auto const& variable : program.getGlobalIntegerVariables()) {
        stream << "global " << variable << '\n';
    }
    stream << '\n';

    for (auto const& formula : program.getFormulas()) {
        stream << formula << '\n';
    }
    stream << '\n';

    for (auto const& module : program.getModules()) {
        stream << module << '\n';
    }

    for (auto const& rewardModel : program.getRewardModels()) {
        stream << rewardModel << '\n';
    }

    for (auto const& label : program.getLabels()) {
        stream << label << '\n';
    }

    if (program.hasInitialConstruct()) {
        stream << program.getInitialConstruct() << '\n';
    }

    if (program.specifiesSystemComposition()) {
        stream << program.getSystemCompositionConstruct();
    }

    return stream;
}

}  // namespace prism
}  // namespace storm
