#include "ResourceSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm {
namespace settings {
namespace modules {

const std::string ResourceSettings::moduleName = "resources";
const std::string ResourceSettings::timeoutOptionName = "timeout";
const std::string ResourceSettings::timeoutOptionShortName = "t";
const std::string ResourceSettings::printTimeAndMemoryOptionName = "timemem";
const std::string ResourceSettings::printTimeAndMemoryOptionShortName = "tm";
const std::string ResourceSettings::signalWaitingTimeOptionName = "signal-timeout";

ResourceSettings::ResourceSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, timeoutOptionName, false, "If given, computation will abort after the timeout has been reached.")
                        .setIsAdvanced()
                        .setShortName(timeoutOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("time", "Seconds after which to timeout.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, printTimeAndMemoryOptionName, false, "Prints CPU time and memory consumption at the end.")
                        .setShortName(printTimeAndMemoryOptionShortName)
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, signalWaitingTimeOptionName, false,
                                                   "Specifies how much time can pass until termination when receiving a termination signal.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("time", "Seconds after which to exit the program.")
                                         .setDefaultValueUnsignedInteger(3)
                                         .build())
                        .build());
}

bool ResourceSettings::isTimeoutSet() const {
    return this->getOption(timeoutOptionName).getHasOptionBeenSet();
}

uint_fast64_t ResourceSettings::getTimeoutInSeconds() const {
    return this->getOption(timeoutOptionName).getArgumentByName("time").getValueAsUnsignedInteger();
}

bool ResourceSettings::isPrintTimeAndMemorySet() const {
    return this->getOption(printTimeAndMemoryOptionName).getHasOptionBeenSet();
}

uint_fast64_t ResourceSettings::getSignalWaitingTimeInSeconds() const {
    return this->getOption(signalWaitingTimeOptionName).getArgumentByName("time").getValueAsUnsignedInteger();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
