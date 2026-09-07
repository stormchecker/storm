#include "Module.h"

#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace prism {
Module::Module(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
               std::vector<storm::prism::IntegerVariable> const& integerVariables, std::vector<storm::prism::ClockVariable> const& clockVariables,
               storm::expressions::Expression const& invariant, std::vector<storm::prism::Command> const& commands, std::string const& filename,
               uint_fast64_t lineNumber)
    : Module(moduleName, booleanVariables, integerVariables, clockVariables, invariant, commands, "", storm::prism::ModuleRenaming(), filename, lineNumber) {
    // Intentionally left empty.
}

Module::Module(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
               std::vector<storm::prism::IntegerVariable> const& integerVariables, std::vector<storm::prism::ClockVariable> const& clockVariables,
               storm::expressions::Expression const& invariant, std::vector<storm::prism::Command> const& commands, std::string const& renamedFromModule,
               storm::prism::ModuleRenaming const& renaming, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      moduleName(moduleName),
      booleanVariables(booleanVariables),
      booleanVariableToIndexMap(),
      integerVariables(integerVariables),
      integerVariableToIndexMap(),
      clockVariables(clockVariables),
      clockVariableToIndexMap(),
      invariant(invariant),
      commands(commands),
      synchronizingActionIndices(),
      actionIndicesToCommandIndexMap(),
      renamedFromModule(renamedFromModule),
      renaming(renaming) {
    // Initialize the internal mappings for fast information retrieval.
    this->createMappings();
}

bool Module::hasUnboundedVariables() const {
    for (auto const& integerVariable : this->integerVariables) {
        if (!integerVariable.hasLowerBoundExpression() || !integerVariable.hasUpperBoundExpression()) {
            return true;
        }
    }
    return false;
}

std::size_t Module::getNumberOfBooleanVariables() const {
    return this->booleanVariables.size();
}

std::size_t Module::getNumberOfIntegerVariables() const {
    return this->integerVariables.size();
}

storm::prism::BooleanVariable const& Module::getBooleanVariable(std::string const& variableName) const {
    auto const& nameIndexPair = this->booleanVariableToIndexMap.find(variableName);
    STORM_LOG_THROW(nameIndexPair != this->booleanVariableToIndexMap.end(), storm::exceptions::InvalidArgumentException,
                    "Unknown boolean variable '" << variableName << "'.");
    return this->getBooleanVariables()[nameIndexPair->second];
}

std::vector<storm::prism::BooleanVariable> const& Module::getBooleanVariables() const {
    return this->booleanVariables;
}

storm::prism::IntegerVariable const& Module::getIntegerVariable(std::string const& variableName) const {
    auto const& nameIndexPair = this->integerVariableToIndexMap.find(variableName);
    STORM_LOG_THROW(nameIndexPair != this->integerVariableToIndexMap.end(), storm::exceptions::InvalidArgumentException,
                    "Unknown integer variable '" << variableName << "'.");
    return this->getIntegerVariables()[nameIndexPair->second];
}

std::vector<storm::prism::IntegerVariable> const& Module::getIntegerVariables() const {
    return this->integerVariables;
}

std::size_t Module::getNumberOfClockVariables() const {
    return this->clockVariables.size();
}

storm::prism::ClockVariable const& Module::getClockVariable(std::string const& variableName) const {
    auto const& nameIndexPair = this->clockVariableToIndexMap.find(variableName);
    STORM_LOG_THROW(nameIndexPair != this->clockVariableToIndexMap.end(), storm::exceptions::InvalidArgumentException,
                    "Unknown clock variable '" << variableName << "'.");
    return this->getClockVariables()[nameIndexPair->second];
}

std::vector<storm::prism::ClockVariable> const& Module::getClockVariables() const {
    return this->clockVariables;
}

std::set<storm::expressions::Variable> Module::getAllExpressionVariables() const {
    std::set<storm::expressions::Variable> result;
    for (auto const& var : this->getBooleanVariables()) {
        result.insert(var.getExpressionVariable());
    }
    for (auto const& var : this->getIntegerVariables()) {
        result.insert(var.getExpressionVariable());
    }
    for (auto const& var : this->getClockVariables()) {
        result.insert(var.getExpressionVariable());
    }
    return result;
}

std::vector<storm::expressions::Expression> Module::getAllRangeExpressions() const {
    std::vector<storm::expressions::Expression> result;
    for (auto const& integerVariable : this->integerVariables) {
        if (integerVariable.hasLowerBoundExpression() || integerVariable.hasUpperBoundExpression()) {
            result.push_back(integerVariable.getRangeExpression());
        }
    }
    return result;
}

std::size_t Module::getNumberOfCommands() const {
    return this->commands.size();
}

std::size_t Module::getNumberOfUpdates() const {
    std::size_t result = 0;
    for (auto const& command : this->getCommands()) {
        result += command.getNumberOfUpdates();
    }
    return result;
}

storm::prism::Command const& Module::getCommand(uint_fast64_t index) const {
    return this->commands[index];
}

std::vector<storm::prism::Command> const& Module::getCommands() const {
    return this->commands;
}

std::vector<storm::prism::Command>& Module::getCommands() {
    return this->commands;
}

std::string const& Module::getName() const {
    return this->moduleName;
}

std::set<uint_fast64_t> const& Module::getSynchronizingActionIndices() const {
    return this->synchronizingActionIndices;
}

bool Module::hasActionIndex(uint_fast64_t actionIndex) const {
    return this->actionIndicesToCommandIndexMap.find(actionIndex) != this->actionIndicesToCommandIndexMap.end();
}

uint64_t Module::getNumberOfUnlabeledCommands() const {
    uint64_t result = 0;
    for (auto const& cmd : commands) {
        if (!cmd.isLabeled()) {
            result++;
        }
    }
    return result;
}

bool Module::isRenamedFromModule() const {
    return this->renamedFromModule != "";
}

std::string const& Module::getBaseModule() const {
    STORM_LOG_THROW(this->isRenamedFromModule(), storm::exceptions::InvalidAccessException,
                    "Unable to retrieve base module of module that was not created by renaming.");
    return this->renamedFromModule;
}

std::map<std::string, std::string> const& Module::getRenaming() const {
    STORM_LOG_THROW(this->isRenamedFromModule(), storm::exceptions::InvalidAccessException,
                    "Unable to retrieve renaming of module that was not created by renaming.");
    return this->renaming.getRenaming();
}

std::set<uint_fast64_t> const& Module::getCommandIndicesByActionIndex(uint_fast64_t actionIndex) const {
    auto actionIndicesCommandSetPair = this->actionIndicesToCommandIndexMap.find(actionIndex);
    if (actionIndicesCommandSetPair != this->actionIndicesToCommandIndexMap.end()) {
        return actionIndicesCommandSetPair->second;
    }

    STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException, "Action index '" << actionIndex << "' does not exist in module.");
}

void Module::createMappings() {
    // Clear the current mappings.
    this->actionIndicesToCommandIndexMap.clear();
    this->booleanVariableToIndexMap.clear();
    this->integerVariableToIndexMap.clear();

    // Create the mappings for the variables.
    for (uint_fast64_t i = 0; i < this->booleanVariables.size(); ++i) {
        this->booleanVariableToIndexMap[this->getBooleanVariables()[i].getName()] = i;
    }
    for (uint_fast64_t i = 0; i < this->integerVariables.size(); ++i) {
        this->integerVariableToIndexMap[this->getIntegerVariables()[i].getName()] = i;
    }
    for (uint_fast64_t i = 0; i < this->clockVariables.size(); ++i) {
        this->booleanVariableToIndexMap[this->getClockVariables()[i].getName()] = i;
    }

    // Add the mapping for all commands.
    for (uint_fast64_t i = 0; i < this->commands.size(); i++) {
        if (this->commands[i].isLabeled()) {
            uint_fast64_t actionIndex = this->commands[i].getActionIndex();
            if (this->actionIndicesToCommandIndexMap.find(actionIndex) == this->actionIndicesToCommandIndexMap.end()) {
                this->actionIndicesToCommandIndexMap.emplace(actionIndex, std::set<uint_fast64_t>());
            }
            this->actionIndicesToCommandIndexMap[actionIndex].insert(i);

            // Only take the command into the set if it's non-synchronizing.
            if (actionIndex != 0) {
                this->synchronizingActionIndices.insert(actionIndex);
            }
        }
    }

    // For all actions that are "in the module", but for which no command exists, we add the mapping to an empty
    // set of commands.
    for (auto const& actionIndex : this->synchronizingActionIndices) {
        if (this->actionIndicesToCommandIndexMap.find(actionIndex) == this->actionIndicesToCommandIndexMap.end()) {
            this->actionIndicesToCommandIndexMap[actionIndex] = std::set<uint_fast64_t>();
        }
    }
}

Module Module::restrictCommands(storm::storage::FlatSet<uint_fast64_t> const& indexSet) const {
    // First construct the new vector of commands.
    std::vector<storm::prism::Command> newCommands;
    for (auto const& command : commands) {
        if (indexSet.find(command.getGlobalIndex()) != indexSet.end()) {
            newCommands.push_back(command);
        }
    }

    return Module(this->getName(), this->getBooleanVariables(), this->getIntegerVariables(), this->getClockVariables(), this->getInvariant(), newCommands);
}

Module Module::restrictActionIndices(storm::storage::FlatSet<uint_fast64_t> const& actionIndices) const {
    // First construct the new vector of commands.
    std::vector<storm::prism::Command> newCommands;
    for (auto const& command : commands) {
        if (actionIndices.find(command.getActionIndex()) != actionIndices.end()) {
            newCommands.push_back(command);
        }
    }

    return Module(this->getName(), this->getBooleanVariables(), this->getIntegerVariables(), this->getClockVariables(), this->getInvariant(), newCommands);
}

Module Module::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    std::vector<BooleanVariable> newBooleanVariables;
    newBooleanVariables.reserve(this->getNumberOfBooleanVariables());
    for (auto const& booleanVariable : this->getBooleanVariables()) {
        newBooleanVariables.emplace_back(booleanVariable.substitute(substitution));
    }

    std::vector<IntegerVariable> newIntegerVariables;
    newBooleanVariables.reserve(this->getNumberOfIntegerVariables());
    for (auto const& integerVariable : this->getIntegerVariables()) {
        newIntegerVariables.emplace_back(integerVariable.substitute(substitution));
    }

    std::vector<Command> newCommands;
    newCommands.reserve(this->getNumberOfCommands());
    for (auto const& command : this->getCommands()) {
        newCommands.emplace_back(command.substitute(substitution));
    }

    return Module(this->getName(), newBooleanVariables, newIntegerVariables, this->getClockVariables(), this->getInvariant(), newCommands, this->getFilename(),
                  this->getLineNumber());
}

Module Module::substituteNonStandardPredicates() const {
    std::vector<BooleanVariable> newBooleanVariables;
    newBooleanVariables.reserve(this->getNumberOfBooleanVariables());
    for (auto const& booleanVariable : this->getBooleanVariables()) {
        newBooleanVariables.emplace_back(booleanVariable.substituteNonStandardPredicates());
    }

    std::vector<IntegerVariable> newIntegerVariables;
    newBooleanVariables.reserve(this->getNumberOfIntegerVariables());
    for (auto const& integerVariable : this->getIntegerVariables()) {
        newIntegerVariables.emplace_back(integerVariable.substituteNonStandardPredicates());
    }

    std::vector<Command> newCommands;
    newCommands.reserve(this->getNumberOfCommands());
    for (auto const& command : this->getCommands()) {
        newCommands.emplace_back(command.substituteNonStandardPredicates());
    }

    return Module(this->getName(), newBooleanVariables, newIntegerVariables, this->getClockVariables(), this->getInvariant(), newCommands, this->getFilename(),
                  this->getLineNumber());
}

Module Module::labelUnlabelledCommands(std::map<uint64_t, std::string> const& suggestions, uint64_t& newId,
                                       std::map<std::string, uint64_t>& nameToIdMapping) const {
    std::vector<Command> newCommands;
    newCommands.reserve(this->getNumberOfCommands());
    for (auto const& command : this->getCommands()) {
        if (command.isLabeled()) {
            newCommands.push_back(command);
        } else {
            if (suggestions.count(command.getGlobalIndex())) {
                std::string newActionName = suggestions.at(command.getGlobalIndex());
                auto it = nameToIdMapping.find(newActionName);
                uint64_t actionId = newId;
                if (it == nameToIdMapping.end()) {
                    nameToIdMapping[newActionName] = newId;
                    newId++;
                } else {
                    actionId = it->second;
                }
                newCommands.emplace_back(command.getGlobalIndex(), command.isMarkovian(), actionId, newActionName, command.getGuardExpression(),
                                         command.getUpdates(), command.getFilename(), command.getLineNumber());

            } else {
                std::string newActionName = getName() + "_cmd_" + std::to_string(command.getGlobalIndex());
                newCommands.emplace_back(command.getGlobalIndex(), command.isMarkovian(), newId, newActionName, command.getGuardExpression(),
                                         command.getUpdates(), command.getFilename(), command.getLineNumber());
                nameToIdMapping[newActionName] = newId;
                newId++;
            }
        }
    }
    return Module(this->getName(), booleanVariables, integerVariables, clockVariables, invariant, newCommands, this->getFilename(), this->getLineNumber());
}

bool Module::containsVariablesOnlyInUpdateProbabilities(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const {
    for (auto const& booleanVariable : this->getBooleanVariables()) {
        if (booleanVariable.hasInitialValue() && booleanVariable.getInitialValueExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }
    for (auto const& integerVariable : this->getIntegerVariables()) {
        if (integerVariable.hasInitialValue() && integerVariable.getInitialValueExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
        if (integerVariable.hasLowerBoundExpression() && integerVariable.getLowerBoundExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
        if (integerVariable.hasUpperBoundExpression() && integerVariable.getUpperBoundExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }

    for (auto const& command : this->getCommands()) {
        if (!command.containsVariablesOnlyInUpdateProbabilities(undefinedConstantVariables)) {
            return false;
        }
    }

    return true;
}

void Module::createMissingInitialValues() {
    for (auto& variable : booleanVariables) {
        variable.createMissingInitialValue();
    }
    for (auto& variable : integerVariables) {
        variable.createMissingInitialValue();
    }
    for (auto& variable : clockVariables) {
        variable.createMissingInitialValue();
    }
}

void Module::removeVariableInitialization() {
    for (auto& variable : booleanVariables) {
        variable.setInitialValueExpression(expressions::Expression());
    }
    for (auto& variable : integerVariables) {
        variable.setInitialValueExpression(expressions::Expression());
    }
    for (auto& variable : clockVariables) {
        variable.setInitialValueExpression(expressions::Expression());
    }
}

bool Module::hasInvariant() const {
    return this->invariant.isInitialized();
}

storm::expressions::Expression const& Module::getInvariant() const {
    return this->invariant;
}

std::ostream& operator<<(std::ostream& stream, Module const& module) {
    stream << "module " << module.getName() << '\n';
    for (auto const& booleanVariable : module.getBooleanVariables()) {
        stream << "\t" << booleanVariable << '\n';
    }
    for (auto const& integerVariable : module.getIntegerVariables()) {
        stream << "\t" << integerVariable << '\n';
    }
    for (auto const& clockVariable : module.getClockVariables()) {
        stream << "\t" << clockVariable << '\n';
    }
    for (auto const& command : module.getCommands()) {
        stream << "\t" << command << '\n';
    }
    stream << "endmodule\n";
    return stream;
}

}  // namespace prism
}  // namespace storm
