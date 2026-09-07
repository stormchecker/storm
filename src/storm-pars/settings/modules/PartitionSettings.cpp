#include "PartitionSettings.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm::settings::modules {

const std::string PartitionSettings::moduleName = "partitioning";
const std::string requestedCoverageOptionName = "terminationCondition";
const std::string printNoIllustrationOptionName = "noillustration";
const std::string printFullResultOptionName = "printfullresult";

PartitionSettings::PartitionSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, requestedCoverageOptionName, false, "The requested coverage")
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("coverage-threshold",
                                                                                            "The fraction of unknown area falls below this threshold.")
                                         .setDefaultValueDouble(0.05)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createIntegerArgument(
                                         "depth-limit", "(advanced) If given, limits the number of times a region is refined.")
                                         .setDefaultValueInteger(-1)
                                         .makeOptional()
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, printNoIllustrationOptionName, false, "If set, no illustration of the result is printed.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, printFullResultOptionName, false, "If set, the full result for every region is printed.").build());
}

double PartitionSettings::getCoverageThreshold() const {
    return this->getOption(requestedCoverageOptionName).getArgumentByName("coverage-threshold").getValueAsDouble();
}

bool PartitionSettings::isDepthLimitSet() const {
    return this->getOption(requestedCoverageOptionName).getArgumentByName("depth-limit").getHasBeenSet() &&
           this->getOption(requestedCoverageOptionName).getArgumentByName("depth-limit").getValueAsInteger() >= 0;
}

bool PartitionSettings::isPrintNoIllustrationSet() const {
    return this->getOption(printNoIllustrationOptionName).getHasOptionBeenSet();
}

bool PartitionSettings::isPrintFullResultSet() const {
    return this->getOption(printFullResultOptionName).getHasOptionBeenSet();
}

uint64_t PartitionSettings::getDepthLimit() const {
    int64_t depth = this->getOption(requestedCoverageOptionName).getArgumentByName("depth-limit").getValueAsInteger();
    STORM_LOG_THROW(depth >= 0, storm::exceptions::InvalidOperationException, "Tried to retrieve the depth limit but it was not set.");
    return (uint64_t)depth;
}

}  // namespace storm::settings::modules