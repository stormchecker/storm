#include "storm/storage/sparse/PrismChoiceOrigins.h"

#include "storm/adapters/JsonAdapter.h"

#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace storage {
namespace sparse {

PrismChoiceOrigins::PrismChoiceOrigins(std::shared_ptr<storm::prism::Program const> const& prismProgram,
                                       std::vector<uint_fast64_t> const& indexToIdentifierMapping, std::vector<CommandSet> const& identifierToCommandSetMapping)
    : ChoiceOrigins(indexToIdentifierMapping), program(prismProgram), identifierToCommandSet(identifierToCommandSetMapping) {
    STORM_LOG_THROW(identifierToCommandSet[this->getIdentifierForChoicesWithNoOrigin()].empty(), storm::exceptions::InvalidArgumentException,
                    "The given command set for the choices without origin is non-empty.");
}

PrismChoiceOrigins::PrismChoiceOrigins(std::shared_ptr<storm::prism::Program const> const& prismProgram, std::vector<uint_fast64_t>&& indexToIdentifierMapping,
                                       std::vector<CommandSet>&& identifierToCommandSetMapping)
    : ChoiceOrigins(std::move(indexToIdentifierMapping)), program(prismProgram), identifierToCommandSet(std::move(identifierToCommandSetMapping)) {
    STORM_LOG_THROW(identifierToCommandSet[this->getIdentifierForChoicesWithNoOrigin()].empty(), storm::exceptions::InvalidArgumentException,
                    "The given command set for the choices without origin is non-empty.");
}

bool PrismChoiceOrigins::isPrismChoiceOrigins() const {
    return true;
}

uint_fast64_t PrismChoiceOrigins::getNumberOfIdentifiers() const {
    return identifierToCommandSet.size();
}

storm::prism::Program const& PrismChoiceOrigins::getProgram() const {
    return *program;
}

PrismChoiceOrigins::CommandSet const& PrismChoiceOrigins::getCommandSet(uint_fast64_t choiceIndex) const {
    return identifierToCommandSet[this->getIdentifier(choiceIndex)];
}

std::shared_ptr<ChoiceOrigins> PrismChoiceOrigins::cloneWithNewIndexToIdentifierMapping(std::vector<uint_fast64_t>&& indexToIdentifierMapping) const {
    auto result = std::make_shared<PrismChoiceOrigins>(this->program, std::move(indexToIdentifierMapping), std::move(this->identifierToCommandSet));
    result->identifierToInfo = this->identifierToInfo;
    return result;
}

void PrismChoiceOrigins::computeIdentifierInfos() const {
    this->identifierToInfo.clear();
    this->identifierToInfo.reserve(this->getNumberOfIdentifiers());
    for (CommandSet const& set : identifierToCommandSet) {
        // Get a string representation of this command set.
        std::stringstream setName;
        if (set.empty()) {
            setName << "No origin";
        } else {
            // process the first command in the set
            auto commandIndexIt = set.begin();
            auto moduleCommandPair = program->getModuleCommandIndexByGlobalCommandIndex(*commandIndexIt);
            storm::prism::Module const& firstModule = program->getModule(moduleCommandPair.first);
            storm::prism::Command const& firstCommand = firstModule.getCommand(moduleCommandPair.second);

            std::string actionName = firstCommand.getActionName();
            setName << actionName;

            // If the commands are labeled, it is clear which modules induced the current choice.
            // In this case, we only need to print more information if there are multiple commands in a module with the same label.
            bool setNameNeedsAllModuleNames = !firstCommand.isLabeled();
            bool actionNameIsUniqueInModule = firstCommand.isLabeled() && firstModule.getCommandIndicesByActionIndex(firstCommand.getActionIndex()).size() == 1;

            bool setNameHasModuleDetails = false;
            if (!actionNameIsUniqueInModule || setNameNeedsAllModuleNames) {
                setNameHasModuleDetails = true;
                if (!actionName.empty()) {
                    setName << " ";
                }
                setName << "(" << firstModule.getName();
                if (!actionNameIsUniqueInModule) {
                    setName << ": " << firstCommand;
                }
            }

            // now process the remaining commands
            for (++commandIndexIt; commandIndexIt != set.end(); ++commandIndexIt) {
                moduleCommandPair = program->getModuleCommandIndexByGlobalCommandIndex(*commandIndexIt);
                storm::prism::Module const& module = program->getModule(moduleCommandPair.first);
                storm::prism::Command const& command = module.getCommand(moduleCommandPair.second);
                STORM_LOG_THROW(command.getActionName() == actionName, storm::exceptions::UnexpectedException, "Inconsistent action names for choice origin.");

                actionNameIsUniqueInModule = firstCommand.isLabeled() && module.getCommandIndicesByActionIndex(command.getActionIndex()).size() == 1;
                if (!actionNameIsUniqueInModule || setNameNeedsAllModuleNames) {
                    if (setNameHasModuleDetails) {
                        setName << "  ||  ";
                    } else {
                        setNameHasModuleDetails = true;
                        if (!actionName.empty()) {
                            setName << " ";
                        }
                        setName << "(";
                    }
                    setName << module.getName();
                    if (!actionNameIsUniqueInModule) {
                        setName << ": " << command;
                    }
                }
            }
            if (setNameHasModuleDetails) {
                setName << ")";
            }
        }
        this->identifierToInfo.push_back(setName.str());
    }
    STORM_LOG_DEBUG("Generated the following names for the choice origins: " << storm::utility::vector::toString(this->identifierToInfo));
    STORM_LOG_ASSERT(storm::utility::vector::isUnique(this->identifierToInfo), "The generated names for the prism choice origins are not unique.");
}

void PrismChoiceOrigins::computeIdentifierJson() const {
    this->identifierToJson.clear();
    this->identifierToJson.reserve(this->getNumberOfIdentifiers());
    for (CommandSet const& set : identifierToCommandSet) {
        // Get a string representation of this command set.
        Json setJson;
        if (set.empty()) {
            setJson = "No origin";
        } else {
            bool first = true;
            std::vector<Json> commandsJson;
            for (auto const& commandIndex : set) {
                Json commandJson;
                auto moduleCommandPair = program->getModuleCommandIndexByGlobalCommandIndex(commandIndex);
                storm::prism::Module const& module = program->getModule(moduleCommandPair.first);
                storm::prism::Command const& command = module.getCommand(moduleCommandPair.second);
                if (first) {
                    setJson["action-label"] = command.getActionName();
                    first = false;
                }
                commandJson["module"] = module.getName();
                commandJson["guard"] = command.getGuardExpression().toString();
                std::vector<Json> updatesJson;
                for (auto const& update : command.getUpdates()) {
                    Json updateJson;
                    updateJson["prob"] = update.getLikelihoodExpression().toString();
                    std::stringstream assignmentsString;
                    bool firstAssignment = true;
                    for (auto const& a : update.getAssignments()) {
                        if (firstAssignment) {
                            firstAssignment = false;
                        } else {
                            assignmentsString << " & ";
                        }
                        assignmentsString << a;
                    }
                    updateJson["result"] = assignmentsString.str();
                    updatesJson.push_back(std::move(updateJson));
                }
                commandJson["updates"] = updatesJson;
                commandsJson.push_back(std::move(commandJson));
            }
            setJson["transitions"] = commandsJson;
        }
        this->identifierToJson.push_back(std::move(setJson));
    }
}

std::size_t PrismChoiceOrigins::hash() const {
    return 0;
}
}  // namespace sparse
}  // namespace storage
}  // namespace storm
