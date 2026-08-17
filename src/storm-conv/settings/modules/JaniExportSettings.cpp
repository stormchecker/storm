#include "JaniExportSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

#include <boost/algorithm/string.hpp>

namespace storm {
namespace settings {
namespace modules {
const std::string JaniExportSettings::moduleName = "exportJani";

const std::string JaniExportSettings::edgeAssignmentsOptionName = "edge-assignments";
const std::string JaniExportSettings::exportFlattenOptionName = "flatten";
const std::string JaniExportSettings::locationVariablesOptionName = "location-variables";
const std::string JaniExportSettings::globalVariablesOptionName = "globalvars";
const std::string JaniExportSettings::localVariablesOptionName = "localvars";
const std::string JaniExportSettings::compactJsonOptionName = "compactjson";
const std::string JaniExportSettings::eliminateArraysOptionName = "remove-arrays";
const std::string JaniExportSettings::eliminateFunctionsOptionName = "remove-functions";
const std::string JaniExportSettings::replaceUnassignedVariablesWithConstantsOptionName = "replace-unassigned-vars";
const std::string JaniExportSettings::simplifyCompositionOptionName = "simplify-composition";
const std::string JaniExportSettings::performLocationElimination = "location-elimination";

JaniExportSettings::JaniExportSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, locationVariablesOptionName, true, "Variables to export in the location")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         "variables", "A comma separated list of automaton and local variable names seperated by a dot, e.g. A.x,B.y.")
                                         .setDefaultValueString("")
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, edgeAssignmentsOptionName, false,
            "If set, the output model can have transient edge assignments. This can simplify the jani model but is not compliant to the jani standard.")
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportFlattenOptionName, false,
                                                   "Flattens the composition of Automata to obtain an equivalent model that contains exactly one automaton")
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, globalVariablesOptionName, false,
                                       "If set, variables will preferably be made global, e.g., to guarantee the same variable order as in the input file.")
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, localVariablesOptionName, false, "If set, variables will preferably be made local.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, compactJsonOptionName, false,
                                                   "If set, the size of the resulting jani file will be reduced at the cost of (human-)readability.")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, eliminateArraysOptionName, false,
                                                   "If set, transforms the model such that array variables/expressions are eliminated.")
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, eliminateFunctionsOptionName, false, "If set, transforms the model such that functions are eliminated.")
            .build());
    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, replaceUnassignedVariablesWithConstantsOptionName, false,
            "If set, local and global variables that are (a) not assigned to some value and (b) have a known initial value are replaced by constants.")
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, simplifyCompositionOptionName, false, "If set, attempts to simplify the system composition.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, performLocationElimination, false,
                                                   "If set, location elimination will be performed before the model is built.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "location-heuristic", "If this number of locations is reached, no further unfolding will be performed")
                                         .setDefaultValueUnsignedInteger(10)
                                         .makeOptional()
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "edges-heuristic", "Determines how many new edges may be created by a single elimination")
                                         .setDefaultValueUnsignedInteger(10000)
                                         .makeOptional()
                                         .build())
                        .build());
}

bool JaniExportSettings::isAllowEdgeAssignmentsSet() const {
    return this->getOption(edgeAssignmentsOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isExportFlattenedSet() const {
    return this->getOption(exportFlattenOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isLocationVariablesSet() const {
    return this->getOption(locationVariablesOptionName).getHasOptionBeenSet();
}

std::vector<std::pair<std::string, std::string>> JaniExportSettings::getLocationVariables() const {
    std::vector<std::pair<std::string, std::string>> result;
    if (isLocationVariablesSet()) {
        std::string argument = this->getOption(locationVariablesOptionName).getArgumentByName("variables").getValueAsString();
        std::vector<std::string> arguments;
        boost::split(arguments, argument, boost::is_any_of(","));
        for (auto const& pair : arguments) {
            std::vector<std::string> keyvaluepair;
            boost::split(keyvaluepair, pair, boost::is_any_of("."));
            STORM_LOG_THROW(keyvaluepair.size() == 2, storm::exceptions::IllegalArgumentException,
                            "Expected a name of the form AUTOMATON.VARIABLE (with no further dots) but got " << pair << ".");
            result.emplace_back(keyvaluepair.at(0), keyvaluepair.at(1));
        }
    }
    return result;
}

bool JaniExportSettings::isGlobalVarsSet() const {
    return this->getOption(globalVariablesOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isLocalVarsSet() const {
    return this->getOption(localVariablesOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isCompactJsonSet() const {
    return this->getOption(compactJsonOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isEliminateArraysSet() const {
    return this->getOption(eliminateArraysOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isEliminateFunctionsSet() const {
    return this->getOption(eliminateFunctionsOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isReplaceUnassignedVariablesWithConstantsSet() const {
    return this->getOption(replaceUnassignedVariablesWithConstantsOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isSimplifyCompositionSet() const {
    return this->getOption(simplifyCompositionOptionName).getHasOptionBeenSet();
}

bool JaniExportSettings::isLocationEliminationSet() const {
    return this->getOption(performLocationElimination).getHasOptionBeenSet();
}

uint64_t JaniExportSettings::getLocationEliminationLocationHeuristic() const {
    return this->getOption(performLocationElimination).getArgumentByName("location-heuristic").getValueAsUnsignedInteger();
}

uint64_t JaniExportSettings::getLocationEliminationEdgesHeuristic() const {
    return this->getOption(performLocationElimination).getArgumentByName("edges-heuristic").getValueAsUnsignedInteger();
}

void JaniExportSettings::finalize() {}

bool JaniExportSettings::check() const {
    return true;
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
