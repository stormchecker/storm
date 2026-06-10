#include "storm/settings/SettingsManager.h"

#include <boost/algorithm/string.hpp>
#include <boost/io/ios_state.hpp>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <set>

#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/OptionParserException.h"
#include "storm/io/file.h"
#include "storm/settings/Option.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/settings/modules/ConditionalSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/CuddSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/settings/modules/ExplorationSettings.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/GlpkSettings.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/settings/modules/GurobiSettings.h"
#include "storm/settings/modules/HintSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/LongRunAverageSolverSettings.h"
#include "storm/settings/modules/MinMaxEquationSolverSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/settings/modules/MultiplierSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/settings/modules/OviSolverSettings.h"
#include "storm/settings/modules/ResourceSettings.h"
#include "storm/settings/modules/Smt2SmtSolverSettings.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/settings/modules/TimeBoundedSolverSettings.h"
#include "storm/settings/modules/TopologicalEquationSolverSettings.h"
#include "storm/settings/modules/TransformationSettings.h"
#include "storm/utility/macros.h"
#include "storm/utility/string.h"

namespace storm {
namespace settings {

SettingsManager::SettingsManager() : modules(), longNameToOptions(), shortNameToOptions(), moduleOptions() {}

SettingsManager::~SettingsManager() {
    // Intentionally left empty.
}

SettingsManager& SettingsManager::manager() {
    static SettingsManager settingsManager;
    return settingsManager;
}

void SettingsManager::setName(std::string const& name, std::string const& executableName) {
    this->name = name;
    this->executableName = executableName;
}

void SettingsManager::setFromCommandLine(int const argc, char const* const argv[]) {
    // We convert the arguments to a vector of strings and strip off the first element since it refers to the
    // name of the program.
    std::vector<std::string> argumentVector(argc - 1);
    for (int i = 1; i < argc; ++i) {
        argumentVector[i - 1] = std::string(argv[i]);
    }

    this->setFromExplodedString(argumentVector);
}

void SettingsManager::setFromString(std::string const& commandLineString) {
    if (commandLineString.empty()) {
        this->setFromExplodedString({});
    } else {
        std::vector<std::string> argumentVector;
        boost::split(argumentVector, commandLineString, boost::is_any_of("\t "));
        this->setFromExplodedString(argumentVector);
    }
}

void SettingsManager::handleUnknownOption(std::string const& optionName, bool isShort) const {
    std::string optionNameWithDashes = (isShort ? "-" : "--") + optionName;
    storm::utility::string::SimilarStrings similarStrings(optionNameWithDashes, 0.6, false);
    std::map<std::string, std::vector<std::string>> similarOptionNames;
    for (auto const& longOption : longNameToOptions) {
        if (similarStrings.add("--" + longOption.first)) {
            similarOptionNames["--" + longOption.first].push_back(longOption.first);
        }
    }
    for (auto const& shortOption : shortNameToOptions) {
        if (similarStrings.add("-" + shortOption.first)) {
            for (auto const& option : shortOption.second) {
                similarOptionNames["-" + shortOption.first].push_back(option->getLongName());
            }
        }
    }
    std::string errorMessage = "Unknown option '" + optionNameWithDashes + "'.";
    if (!similarOptionNames.empty()) {
        errorMessage += " " + similarStrings.toDidYouMeanString() + "\n\n";
        std::vector<std::string> sortedSimilarOptionNames;
        auto similarStringsList = similarStrings.toList();
        for (auto const& s : similarStringsList) {
            for (auto const& longOptionName : similarOptionNames.at(s)) {
                sortedSimilarOptionNames.push_back(longOptionName);
            }
        }
        errorMessage += getHelpForSelection({}, sortedSimilarOptionNames, "", "##### Suggested options:");
    }
    STORM_LOG_THROW(false, storm::exceptions::OptionParserException, errorMessage);
}

void SettingsManager::setFromExplodedString(std::vector<std::string> const& commandLineArguments) {
    // In order to assign the parsed arguments to an option, we need to keep track of the "active" option's name.
    bool optionActive = false;
    bool activeOptionIsShortName = false;
    std::string activeOptionName = "";
    std::vector<std::string> argumentCache;

    // Walk through all arguments.
    for (uint_fast64_t i = 0; i < commandLineArguments.size(); ++i) {
        std::string const& currentArgument = commandLineArguments[i];

        // Check if the given argument is a new option or belongs to a previously given option.
        if (!currentArgument.empty() && currentArgument.at(0) == '-') {
            if (optionActive) {
                // At this point we know that a new option is about to come. Hence, we need to assign the current
                // cache content to the option that was active until now.
                setOptionsArguments(activeOptionName, activeOptionIsShortName ? this->shortNameToOptions : this->longNameToOptions, argumentCache);

                // After the assignment, the argument cache needs to be cleared.
                argumentCache.clear();
            } else {
                optionActive = true;
            }

            if (currentArgument.at(1) == '-') {
                // In this case, the argument has to be the long name of an option. Try to get all options that
                // match the long name.
                std::string optionName = currentArgument.substr(2);
                auto optionIterator = this->longNameToOptions.find(optionName);
                if (optionIterator == this->longNameToOptions.end()) {
                    handleUnknownOption(optionName, false);
                }
                activeOptionIsShortName = false;
                activeOptionName = optionName;
            } else {
                // In this case, the argument has to be the short name of an option. Try to get all options that
                // match the short name.
                std::string optionName = currentArgument.substr(1);
                auto optionIterator = this->shortNameToOptions.find(optionName);
                if (optionIterator == this->shortNameToOptions.end()) {
                    handleUnknownOption(optionName, true);
                }
                activeOptionIsShortName = true;
                activeOptionName = optionName;
            }
        } else if (optionActive) {
            // Add the current argument to the list of arguments for the currently active options.
            argumentCache.push_back(currentArgument);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::OptionParserException,
                            "Found stray argument '" << currentArgument << "' that is not preceeded by a matching option.");
        }
    }

    // If an option is still active at this point, we need to set it.
    if (optionActive) {
        setOptionsArguments(activeOptionName, activeOptionIsShortName ? this->shortNameToOptions : this->longNameToOptions, argumentCache);
    }

    // Include the options from a possibly specified configuration file, but don't overwrite existing settings.
    if (storm::settings::hasModule<storm::settings::modules::GeneralSettings>() &&
        storm::settings::getModule<storm::settings::modules::GeneralSettings>().isConfigSet()) {
        this->setFromConfigurationFile(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getConfigFilename());
    }

    // Finally, check whether all modules are okay with the current settings.
    this->finalizeAllModules();
}

void SettingsManager::setFromConfigurationFile(std::string const& configFilename) {
    std::map<std::string, std::vector<std::string>> configurationFileSettings = parseConfigFile(configFilename);

    for (auto const& optionArgumentsPair : configurationFileSettings) {
        auto options = this->longNameToOptions.find(optionArgumentsPair.first);

        // We don't need to check whether this option exists or not, because this is already checked when
        // parsing the configuration file.

        // Now go through all the matching options and set them according to the values.
        for (auto option : options->second) {
            if (option->getHasOptionBeenSet()) {
                // If the option was already set from the command line, we issue a warning and ignore the
                // settings from the configuration file.
                STORM_LOG_WARN("The option '" << option->getLongName() << "' of module '" << option->getModuleName()
                                              << "' has been set in the configuration file '" << configFilename
                                              << "', but was overwritten on the command line.\n");
            } else {
                // If, however, the option has not been set yet, we try to assign values ot its arguments
                // based on the argument strings.
                setOptionArguments(optionArgumentsPair.first, option, optionArgumentsPair.second);
            }
        }
    }
    // Finally, check whether all modules are okay with the current settings.
    this->finalizeAllModules();
}

void SettingsManager::printHelp(std::string const& filter) const {
    STORM_PRINT("usage: " << executableName << " [options]\n\n");

    if (filter == "frequent" || filter == "all") {
        bool includeAdvanced = (filter == "all");
        // Find longest option name.
        uint_fast64_t maxLength = getPrintLengthOfLongestOption(includeAdvanced);

        std::vector<std::string> invisibleModules;
        uint64_t numHidden = 0;
        for (auto const& moduleName : this->moduleNames) {
            // Only print for visible modules.
            if (hasModule(moduleName, true)) {
                STORM_PRINT(getHelpForModule(moduleName, maxLength, includeAdvanced));
                // collect 'hidden' options
                if (!includeAdvanced) {
                    auto moduleIterator = moduleOptions.find(moduleName);
                    if (moduleIterator != this->moduleOptions.end()) {
                        bool allAdvanced = true;
                        for (auto const& option : moduleIterator->second) {
                            if (!option->getIsAdvanced()) {
                                allAdvanced = false;
                            } else {
                                ++numHidden;
                            }
                        }
                        if (!moduleIterator->second.empty() && allAdvanced) {
                            invisibleModules.push_back(moduleName);
                        }
                    }
                }
            }
        }
        if (!includeAdvanced) {
            if (numHidden == 1) {
                STORM_PRINT(numHidden << " hidden option.\n");
            } else {
                STORM_PRINT(numHidden << " hidden options.\n");
            }
            if (!invisibleModules.empty()) {
                if (invisibleModules.size() == 1) {
                    STORM_PRINT(invisibleModules.size() << " hidden module (" << boost::join(invisibleModules, ", ") << ").\n");
                } else {
                    STORM_PRINT(invisibleModules.size() << " hidden modules (" << boost::join(invisibleModules, ", ") << ").\n");
                }
            }
            STORM_PRINT("\nType '" + executableName + " --help modulename' to display all options of a specific module.\n");
            STORM_PRINT("Type '" + executableName + " --help all' to display a complete list of options.\n");
        }
    } else {
        // Create a regular expression from the input hint.
        std::regex hintRegex(filter, std::regex_constants::ECMAScript | std::regex_constants::icase);

        // Try to match the regular expression against the known modules.
        std::vector<std::string> matchingModuleNames;
        for (auto const& moduleName : this->moduleNames) {
            if (std::regex_search(moduleName, hintRegex)) {
                if (hasModule(moduleName, true)) {
                    matchingModuleNames.push_back(moduleName);
                }
            }
        }

        // Try to match the regular expression against the known options.
        std::vector<std::string> matchingOptionNames;
        for (auto const& optionName : this->longOptionNames) {
            if (std::regex_search(optionName, hintRegex)) {
                matchingOptionNames.push_back(optionName);
            }
        }

        std::string optionList = getHelpForSelection(matchingModuleNames, matchingOptionNames,
                                                     "Matching modules for filter '" + filter + "':", "Matching options for filter '" + filter + "':");
        if (optionList.empty()) {
            STORM_PRINT("Filter '" << filter << "' did not match any modules or options.\n");
        } else {
            STORM_PRINT(optionList);
        }
    }
}

std::string SettingsManager::getHelpForSelection(std::vector<std::string> const& selectedModuleNames, std::vector<std::string> const& selectedLongOptionNames,
                                                 std::string modulesHeader, std::string optionsHeader) const {
    std::stringstream stream;

    // Remember which options we printed, so we don't display options twice.
    std::set<std::shared_ptr<Option>> printedOptions;

    // Try to match the regular expression against the known modules.
    uint_fast64_t maxLengthModules = 0;
    for (auto const& moduleName : selectedModuleNames) {
        maxLengthModules = std::max(maxLengthModules, getPrintLengthOfLongestOption(moduleName, true));
        // Add all options of this module to the list of printed options so we don't print them twice.
        auto optionIterator = this->moduleOptions.find(moduleName);
        STORM_LOG_ASSERT(optionIterator != this->moduleOptions.end(), "Unable to find selected module " << moduleName << ".");
        printedOptions.insert(optionIterator->second.begin(), optionIterator->second.end());
    }

    // Try to match the regular expression against the known options.
    std::vector<std::shared_ptr<Option>> matchingOptions;
    uint_fast64_t maxLengthOptions = 0;
    for (auto const& optionName : selectedLongOptionNames) {
        auto optionIterator = this->longNameToOptions.find(optionName);
        STORM_LOG_ASSERT(optionIterator != this->longNameToOptions.end(), "Unable to find selected option " << optionName << ".");
        for (auto const& option : optionIterator->second) {
            // Only add the option if we have not already added it to the list of options that is going
            // to be printed anyway.
            if (printedOptions.find(option) == printedOptions.end()) {
                maxLengthOptions = std::max(maxLengthOptions, option->getPrintLength());
                matchingOptions.push_back(option);
                printedOptions.insert(option);
            }
        }
    }

    // Print the matching modules.
    uint_fast64_t maxLength = std::max(maxLengthModules, maxLengthOptions);
    if (selectedModuleNames.size() > 0) {
        if (modulesHeader != "") {
            stream << modulesHeader << '\n';
        }
        for (auto const& matchingModuleName : selectedModuleNames) {
            stream << getHelpForModule(matchingModuleName, maxLength, true);
        }
    }

    // Print the matching options.
    if (matchingOptions.size() > 0) {
        if (optionsHeader != "") {
            stream << optionsHeader << '\n';
        }
        for (auto const& option : matchingOptions) {
            stream << std::setw(maxLength) << std::left << *option << '\n';
        }
    }
    return stream.str();
}

std::string SettingsManager::getHelpForModule(std::string const& moduleName, uint_fast64_t maxLength, bool includeAdvanced) const {
    auto moduleIterator = moduleOptions.find(moduleName);
    if (moduleIterator == this->moduleOptions.end()) {
        return "";
    }
    // STORM_LOG_THROW(moduleIterator != moduleOptions.end(), storm::exceptions::IllegalFunctionCallException, "Cannot print help for unknown module '" <<
    // moduleName << "'.");

    // Check whether there is at least one (enabled) option in this module
    uint64_t numOfOptions = 0;
    for (auto const& option : moduleIterator->second) {
        if (includeAdvanced || !option->getIsAdvanced()) {
            ++numOfOptions;
        }
    }

    std::stringstream stream;
    if (numOfOptions > 0) {
        std::string displayedModuleName = "'" + moduleName + "'";
        if (!includeAdvanced) {
            displayedModuleName += " (" + std::to_string(numOfOptions) + "/" + std::to_string(moduleIterator->second.size()) + " shown)";
        }
        stream << "##### Module " << displayedModuleName << " " << std::string(std::min(maxLength, maxLength - displayedModuleName.length() - 14), '#') << '\n';

        // Save the flags for std::cout so we can manipulate them and be sure they will be restored as soon as this
        // stream goes out of scope.
        boost::io::ios_flags_saver out(std::cout);

        for (auto const& option : moduleIterator->second) {
            if (includeAdvanced || !option->getIsAdvanced()) {
                stream << std::setw(maxLength) << std::left << *option << '\n';
            }
        }
        stream << '\n';
    }
    return stream.str();
}

uint_fast64_t SettingsManager::getPrintLengthOfLongestOption(bool includeAdvanced) const {
    uint_fast64_t length = 0;
    for (auto const& moduleName : this->moduleNames) {
        length = std::max(getPrintLengthOfLongestOption(moduleName, includeAdvanced), length);
    }
    return length;
}

uint_fast64_t SettingsManager::getPrintLengthOfLongestOption(std::string const& moduleName, bool includeAdvanced) const {
    auto moduleIterator = modules.find(moduleName);
    STORM_LOG_THROW(moduleIterator != modules.end(), storm::exceptions::IllegalFunctionCallException,
                    "Unable to retrieve option length of unknown module '" << moduleName << "'.");
    return moduleIterator->second->getPrintLengthOfLongestOption(includeAdvanced);
}

void SettingsManager::addModule(std::unique_ptr<modules::ModuleSettings>&& moduleSettings, bool doRegister) {
    auto moduleIterator = this->modules.find(moduleSettings->getModuleName());
    STORM_LOG_THROW(moduleIterator == this->modules.end(), storm::exceptions::IllegalFunctionCallException,
                    "Unable to register module '" << moduleSettings->getModuleName() << "' because a module with the same name already exists.");

    // Take over the module settings object.
    std::string moduleName = moduleSettings->getModuleName();
    this->moduleNames.push_back(moduleName);
    this->modules.emplace(moduleSettings->getModuleName(), std::move(moduleSettings));
    auto iterator = this->modules.find(moduleName);
    std::unique_ptr<modules::ModuleSettings> const& settings = iterator->second;

    if (doRegister) {
        this->moduleOptions.emplace(moduleName, std::vector<std::shared_ptr<Option>>());
        // Now register the options of the module.
        for (auto const& option : settings->getOptions()) {
            this->addOption(option);
        }
    }
}

void SettingsManager::addOption(std::shared_ptr<Option> const& option) {
    // First, we register to which module the given option belongs.
    auto moduleOptionIterator = this->moduleOptions.find(option->getModuleName());
    STORM_LOG_THROW(moduleOptionIterator != this->moduleOptions.end(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot add option for unknown module '" << option->getModuleName() << "'.");
    moduleOptionIterator->second.emplace_back(option);

    // Then, we add the option's name (and possibly short name) to the registered options. If a module prefix is
    // not required for this option, we have to add both versions to our mappings, the prefixed one and the
    // non-prefixed one.
    if (!option->getRequiresModulePrefix()) {
        bool isCompatible = this->isCompatible(option, option->getLongName(), this->longNameToOptions);
        STORM_LOG_THROW(isCompatible, storm::exceptions::IllegalFunctionCallException,
                        "Unable to add option '" << option->getLongName() << "', because an option with the same name is incompatible with it.");
        addOptionToMap(option->getLongName(), option, this->longNameToOptions);
    }
    // For the prefixed name, we don't need a compatibility check, because a module is not allowed to register the same option twice.
    addOptionToMap(option->getModuleName() + ":" + option->getLongName(), option, this->longNameToOptions);
    longOptionNames.push_back(option->getModuleName() + ":" + option->getLongName());

    if (option->getHasShortName()) {
        if (!option->getRequiresModulePrefix()) {
            bool isCompatible = this->isCompatible(option, option->getShortName(), this->shortNameToOptions);
            STORM_LOG_THROW(isCompatible, storm::exceptions::IllegalFunctionCallException,
                            "Unable to add option '" << option->getLongName() << "', because an option with the same name is incompatible with it.");
            addOptionToMap(option->getShortName(), option, this->shortNameToOptions);
        }
        addOptionToMap(option->getModuleName() + ":" + option->getShortName(), option, this->shortNameToOptions);
    }
}

bool SettingsManager::hasModule(std::string const& moduleName, bool checkHidden) const {
    if (checkHidden) {
        return this->moduleOptions.find(moduleName) != this->moduleOptions.end();
    } else {
        return this->modules.find(moduleName) != this->modules.end();
    }
}

modules::ModuleSettings const& SettingsManager::getModule(std::string const& moduleName) const {
    auto moduleIterator = this->modules.find(moduleName);
    STORM_LOG_THROW(moduleIterator != this->modules.end(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve unknown module '" << moduleName << "'.");
    return *moduleIterator->second;
}

modules::ModuleSettings& SettingsManager::getModule(std::string const& moduleName) {
    auto moduleIterator = this->modules.find(moduleName);
    STORM_LOG_THROW(moduleIterator != this->modules.end(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve unknown module '" << moduleName << "'.");
    return *moduleIterator->second;
}

bool SettingsManager::isCompatible(std::shared_ptr<Option> const& option, std::string const& optionName,
                                   std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> const& optionMap) {
    auto optionIterator = optionMap.find(optionName);
    if (optionIterator != optionMap.end()) {
        for (auto const& otherOption : optionIterator->second) {
            bool locallyCompatible = option->isCompatibleWith(*otherOption);
            if (!locallyCompatible) {
                return false;
            }
        }
    }
    return true;
}

void SettingsManager::setOptionArguments(std::string const& optionName, std::shared_ptr<Option> option, std::vector<std::string> const& argumentCache) {
    STORM_LOG_THROW(argumentCache.size() <= option->getArgumentCount(), storm::exceptions::OptionParserException,
                    "Too many arguments for option '" << optionName << "'.");
    STORM_LOG_THROW(!option->getHasOptionBeenSet(), storm::exceptions::OptionParserException, "Option '" << optionName << "' is set multiple times.");

    // Now set the provided argument values one by one.
    for (uint_fast64_t i = 0; i < argumentCache.size(); ++i) {
        ArgumentBase& argument = option->getArgument(i);
        bool conversionOk = argument.setFromStringValue(argumentCache[i]);
        STORM_LOG_THROW(conversionOk, storm::exceptions::OptionParserException,
                        "Value '" << argumentCache[i] << "' is invalid for argument <" << argument.getName() << "> of option:\n"
                                  << *option);
    }

    // In case there are optional arguments that were not set, we set them to their default value.
    for (uint_fast64_t i = argumentCache.size(); i < option->getArgumentCount(); ++i) {
        ArgumentBase& argument = option->getArgument(i);
        STORM_LOG_THROW(argument.getIsOptional(), storm::exceptions::OptionParserException,
                        "Non-optional argument <" << argument.getName() << "> of option:\n"
                                                  << *option);
        argument.setFromDefaultValue();
    }

    option->setHasOptionBeenSet();
    if (optionName != option->getLongName() && optionName != option->getShortName() && boost::starts_with(optionName, option->getModuleName())) {
        option->setHasOptionBeenSetWithModulePrefix();
    }
}

void SettingsManager::setOptionsArguments(std::string const& optionName, std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> const& optionMap,
                                          std::vector<std::string> const& argumentCache) {
    auto optionIterator = optionMap.find(optionName);
    STORM_LOG_THROW(optionIterator != optionMap.end(), storm::exceptions::OptionParserException, "Unknown option '" << optionName << "'.");

    // Iterate over all options and set the arguments.
    for (auto& option : optionIterator->second) {
        setOptionArguments(optionName, option, argumentCache);
    }
}

void SettingsManager::addOptionToMap(std::string const& name, std::shared_ptr<Option> const& option,
                                     std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>>& optionMap) {
    auto optionIterator = optionMap.find(name);
    if (optionIterator == optionMap.end()) {
        std::vector<std::shared_ptr<Option>> optionVector;
        optionVector.push_back(option);
        optionMap.emplace(name, optionVector);
    } else {
        optionIterator->second.push_back(option);
    }
}

void SettingsManager::finalizeAllModules() {
    for (auto const& nameModulePair : this->modules) {
        nameModulePair.second->finalize();
        nameModulePair.second->check();
    }
}

std::map<std::string, std::vector<std::string>> SettingsManager::parseConfigFile(std::string const& filename) const {
    std::map<std::string, std::vector<std::string>> result;

    std::ifstream input;
    storm::io::openFile(filename, input);

    bool globalScope = true;
    std::string activeModule = "";
    uint_fast64_t lineNumber = 1;
    for (std::string line; storm::io::getline(input, line); ++lineNumber) {
        // If the first character of the line is a "[", we expect the settings of a new module to start and
        // the line to be of the shape [<module>].
        if (line.at(0) == '[') {
            STORM_LOG_THROW(
                line.at(0) == '[' && line.find("]") == line.length() - 1 && line.find("[", 1) == line.npos, storm::exceptions::OptionParserException,
                "Illegal module name header in configuration file '" << filename << " in line " << std::to_string(lineNumber)
                                                                     << ". Expected [<module>] where <module> is a placeholder for a known module.");

            // Extract the module name and check whether it's a legal one.
            std::string moduleName = line.substr(1, line.length() - 2);
            STORM_LOG_THROW(moduleName != "" && (moduleName == "global" || (this->modules.find(moduleName) != this->modules.end())),
                            storm::exceptions::OptionParserException,
                            "Module header in configuration file '" << filename << " in line " << std::to_string(lineNumber) << " refers to unknown module '"
                                                                    << moduleName << ".");

            // If the module name is "global", we unset the currently active module and treat all options to follow as unprefixed.
            if (moduleName == "global") {
                globalScope = true;
            } else {
                activeModule = moduleName;
                globalScope = false;
            }
        } else {
            // In this case, we expect the line to be of the shape o or o=a b c, where o is an option and a, b
            // and c are the values that are supposed to be assigned to the arguments of the option.
            std::size_t assignmentSignIndex = line.find("=");
            bool containsAssignment = false;
            if (assignmentSignIndex != line.npos) {
                containsAssignment = true;
            }

            std::string optionName;
            if (containsAssignment) {
                optionName = line.substr(0, assignmentSignIndex);
            } else {
                optionName = line;
            }

            if (globalScope) {
                STORM_LOG_THROW(this->longNameToOptions.find(optionName) != this->longNameToOptions.end(), storm::exceptions::OptionParserException,
                                "Option assignment in configuration file '" << filename << " in line " << lineNumber << " refers to unknown option '"
                                                                            << optionName << "'.");
            } else {
                STORM_LOG_THROW(this->longNameToOptions.find(activeModule + ":" + optionName) != this->longNameToOptions.end(),
                                storm::exceptions::OptionParserException,
                                "Option assignment in configuration file '" << filename << " in line " << lineNumber << " refers to unknown option '"
                                                                            << activeModule << ":" << optionName << "'.");
            }

            std::string fullOptionName = (!globalScope ? activeModule + ":" : "") + optionName;
            STORM_LOG_WARN_COND(result.find(fullOptionName) == result.end(), "Option '" << fullOptionName << "' is set in line " << lineNumber
                                                                                        << " of configuration file " << filename
                                                                                        << ", but has been set before.");

            // If the current line is an assignment, split the right-hand side of the assignment into parts
            // enclosed by quotation marks.
            if (containsAssignment) {
                std::string assignedValues = line.substr(assignmentSignIndex + 1);
                std::vector<std::string> argumentCache;

                // As horrible as it may look, this regular expression matches either a quoted string (possibly
                // containing escaped quotes) or a simple word (without whitespaces and quotes).
                std::regex argumentRegex("\"(([^\\\\\"]|((\\\\\\\\)*\\\\\")|\\\\[^\"])*)\"|(([^ \\\\\"]|((\\\\\\\\)*\\\\\")|\\\\[^\"])+)");
                boost::algorithm::trim_left(assignedValues);

                while (!assignedValues.empty()) {
                    std::smatch match;
                    bool hasMatch = std::regex_search(assignedValues, match, argumentRegex);

                    // If the input could not be matched, we have a parsing error.
                    STORM_LOG_THROW(
                        hasMatch, storm::exceptions::OptionParserException,
                        "Parsing error in configuration file '" << filename << "' in line " << lineNumber << ". Unexpected input '" << assignedValues << "'.");

                    // Extract the matched argument and cut off the quotation marks if necessary.
                    std::string matchedArgument = std::string(match[0].first, match[0].second);
                    if (matchedArgument.at(0) == '"') {
                        matchedArgument = matchedArgument.substr(1, matchedArgument.length() - 2);
                    }
                    argumentCache.push_back(matchedArgument);

                    assignedValues = assignedValues.substr(match.length());
                    boost::algorithm::trim_left(assignedValues);
                }

                // After successfully parsing the argument values, we store them in the result map.
                result.emplace(fullOptionName, argumentCache);
            } else {
                // In this case, we can just insert the option to indicate it should be set (without arguments).
                result.emplace(fullOptionName, std::vector<std::string>());
            }
        }
    }

    storm::io::closeFile(input);
    return result;
}

SettingsManager const& manager() {
    return SettingsManager::manager();
}

SettingsManager& mutableManager() {
    return SettingsManager::manager();
}

storm::settings::modules::BuildSettings& mutableBuildSettings() {
    return dynamic_cast<storm::settings::modules::BuildSettings&>(mutableManager().getModule(storm::settings::modules::BuildSettings::moduleName));
}

storm::settings::modules::AbstractionSettings& mutableAbstractionSettings() {
    return dynamic_cast<storm::settings::modules::AbstractionSettings&>(mutableManager().getModule(storm::settings::modules::AbstractionSettings::moduleName));
}

void initializeAll(std::string const& name, std::string const& executableName) {
    storm::settings::mutableManager().setName(name, executableName);

    // Register all known settings modules.
    storm::settings::addModule<storm::settings::modules::GeneralSettings>();
    storm::settings::addModule<storm::settings::modules::IOSettings>();
    storm::settings::addModule<storm::settings::modules::BuildSettings>();
    storm::settings::addModule<storm::settings::modules::CoreSettings>();
    storm::settings::addModule<storm::settings::modules::ModelCheckerSettings>();
    storm::settings::addModule<storm::settings::modules::DebugSettings>();
    storm::settings::addModule<storm::settings::modules::CuddSettings>();
    storm::settings::addModule<storm::settings::modules::SylvanSettings>();
    storm::settings::addModule<storm::settings::modules::GmmxxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EigenEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::NativeEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EliminationSettings>();
    storm::settings::addModule<storm::settings::modules::LongRunAverageSolverSettings>();
    storm::settings::addModule<storm::settings::modules::TimeBoundedSolverSettings>();
    storm::settings::addModule<storm::settings::modules::MinMaxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::GameSolverSettings>();
    storm::settings::addModule<storm::settings::modules::BisimulationSettings>();
    storm::settings::addModule<storm::settings::modules::GlpkSettings>();
    storm::settings::addModule<storm::settings::modules::GurobiSettings>();
    storm::settings::addModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::Smt2SmtSolverSettings>();
    storm::settings::addModule<storm::settings::modules::ExplorationSettings>();
    storm::settings::addModule<storm::settings::modules::ResourceSettings>();
    storm::settings::addModule<storm::settings::modules::AbstractionSettings>();
    storm::settings::addModule<storm::settings::modules::MultiObjectiveSettings>();
    storm::settings::addModule<storm::settings::modules::MultiplierSettings>();
    storm::settings::addModule<storm::settings::modules::TransformationSettings>();
    storm::settings::addModule<storm::settings::modules::HintSettings>();
    storm::settings::addModule<storm::settings::modules::OviSolverSettings>();
    storm::settings::addModule<storm::settings::modules::ConditionalSettings>();
}

}  // namespace settings
}  // namespace storm
