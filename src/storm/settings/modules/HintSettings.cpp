#include "HintSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm {
namespace settings {
namespace modules {

const std::string HintSettings::moduleName = "hints";

const std::string stateHintOption = "states";

HintSettings::HintSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, stateHintOption, true, "Estimate of the number of reachable states")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("number", "size.").build())
                        .build());
}

bool HintSettings::isNumberStatesSet() const {
    return this->getOption(stateHintOption).getHasOptionBeenSet();
}

uint64_t HintSettings::getNumberStates() const {
    return this->getOption(stateHintOption).getArgumentByName("number").getValueAsUnsignedInteger();
}

bool HintSettings::check() const {
    return true;
}

void HintSettings::finalize() {
    // Intentionally left empty
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
