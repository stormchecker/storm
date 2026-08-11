#include "storm/settings/modules/EliminationSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string EliminationSettings::moduleName = "elimination";
const std::string EliminationSettings::eliminationMethodOptionName = "method";
const std::string EliminationSettings::eliminationOrderOptionName = "order";
const std::string EliminationSettings::entryStatesLastOptionName = "entrylast";
const std::string EliminationSettings::maximalSccSizeOptionName = "sccsize";
const std::string EliminationSettings::useDedicatedModelCheckerOptionName = "use-dedicated-mc";

EliminationSettings::EliminationSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> orders = {"fw", "fwrev", "bw", "bwrev", "rand", "spen", "dpen", "regex"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, eliminationOrderOptionName, true, "The order that is to be used for the elimination techniques.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the order in which states are chosen for elimination.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(orders))
                             .setDefaultValueString("fwrev")
                             .build())
            .build());

    std::vector<std::string> methods = {"state", "hybrid"};
    this->addOption(storm::settings::OptionBuilder(moduleName, eliminationMethodOptionName, true, "The elimination technique to use.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the elimination technique to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("state")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, entryStatesLastOptionName, true, "Sets whether the entry states are eliminated last.")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, maximalSccSizeOptionName, true, "Sets the maximal size of the SCCs for which state elimination is applied.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("maxsize", "The maximal size of an SCC on which state elimination is applied.")
                    .setDefaultValueUnsignedInteger(20)
                    .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, useDedicatedModelCheckerOptionName, true,
                                                   "Sets whether to use the dedicated model elimination checker (only DTMCs).")
                        .setIsAdvanced()
                        .build());
}

storm::solver::stateelimination::EliminationMethod EliminationSettings::getEliminationMethod() const {
    std::string eliminationMethodAsString = this->getOption(eliminationMethodOptionName).getArgumentByName("name").getValueAsString();
    if (eliminationMethodAsString == "state") {
        return storm::solver::stateelimination::EliminationMethod::State;
    } else if (eliminationMethodAsString == "hybrid") {
        return storm::solver::stateelimination::EliminationMethod::Hybrid;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Illegal elimination method selected.");
    }
}

storm::solver::stateelimination::EliminationOrder EliminationSettings::getEliminationOrder() const {
    std::string eliminationOrderAsString = this->getOption(eliminationOrderOptionName).getArgumentByName("name").getValueAsString();
    if (eliminationOrderAsString == "fw") {
        return storm::solver::stateelimination::EliminationOrder::Forward;
    } else if (eliminationOrderAsString == "fwrev") {
        return storm::solver::stateelimination::EliminationOrder::ForwardReversed;
    } else if (eliminationOrderAsString == "bw") {
        return storm::solver::stateelimination::EliminationOrder::Backward;
    } else if (eliminationOrderAsString == "bwrev") {
        return storm::solver::stateelimination::EliminationOrder::BackwardReversed;
    } else if (eliminationOrderAsString == "rand") {
        return storm::solver::stateelimination::EliminationOrder::Random;
    } else if (eliminationOrderAsString == "spen") {
        return storm::solver::stateelimination::EliminationOrder::StaticPenalty;
    } else if (eliminationOrderAsString == "dpen") {
        return storm::solver::stateelimination::EliminationOrder::DynamicPenalty;
    } else if (eliminationOrderAsString == "regex") {
        return storm::solver::stateelimination::EliminationOrder::RegularExpression;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Illegal elimination order selected.");
    }
}

bool EliminationSettings::isEliminateEntryStatesLastSet() const {
    return this->getOption(entryStatesLastOptionName).getHasOptionBeenSet();
}

uint_fast64_t EliminationSettings::getMaximalSccSize() const {
    return this->getOption(maximalSccSizeOptionName).getArgumentByName("maxsize").getValueAsUnsignedInteger();
}

bool EliminationSettings::isUseDedicatedModelCheckerSet() const {
    return this->getOption(useDedicatedModelCheckerOptionName).getHasOptionBeenSet();
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
