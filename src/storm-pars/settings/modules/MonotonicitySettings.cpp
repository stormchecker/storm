#include "MonotonicitySettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {
const std::string MonotonicitySettings::moduleName = "mon";
const std::string MonotonicitySettings::usePLABounds = "useBounds";
const std::string MonotonicitySettings::sccElimination = "eliminateSCCs";
const std::string MonotonicitySettings::samplesMonotonicityAnalysis = "samples";

const std::string MonotonicitySettings::dotOutput = "dotOutput";
const std::string MonotonicitySettings::exportMonotonicityName = "exportMonotonicity";
const std::string MonotonicitySettings::monSolution = "solutionFunction";
const std::string MonotonicitySettings::monSolutionShortName = "msf";
const std::string MonotonicitySettings::monotonicityThreshold = "depth";

const std::string MonotonicitySettings::monotoneParameters = "parameters";

MonotonicitySettings::MonotonicitySettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, usePLABounds, true, "Sets whether pla bounds should be used for monotonicity analysis")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, sccElimination, true, "Sets whether SCCs should be eliminated in the monotonicity analysis")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, samplesMonotonicityAnalysis, true, "Sets whether monotonicity should be checked on samples")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                             samplesMonotonicityAnalysis, "The number of samples taken in monotonicity-analysis can be given, default is 0, no samples")
                             .setDefaultValueUnsignedInteger(0)
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, monSolution, true, "Sets whether monotonicity should be checked on solution function or reachability order")
            .setIsAdvanced()
            .setShortName(monSolutionShortName)
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, dotOutput, true, "Sets whether a dot output of the ROs is needed")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("dotFilename", "The output file.")
                                         .setDefaultValueString("dotOutput")
                                         .makeOptional()
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportMonotonicityName, true, "Exports the result of monotonicity checking to the given file.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The output file.").build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, monotonicityThreshold, true,
                                                   "Sets for region refinement after which depth whether monotonicity checking should be used.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         monotonicityThreshold, "The depth threshold from which on monotonicity is used for Parameter Lifting")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, monotoneParameters, true, "Sets monotone parameters from file.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("monotoneParametersFilename", "The file where the monotone parameters are set").build())
            .build());
}

bool MonotonicitySettings::isUsePLABoundsSet() const {
    return this->getOption(usePLABounds).getHasOptionBeenSet();
}

bool MonotonicitySettings::isSccEliminationSet() const {
    return this->getOption(sccElimination).getHasOptionBeenSet();
}

bool MonotonicitySettings::isDotOutputSet() const {
    return this->getOption(dotOutput).getHasOptionBeenSet();
}

bool MonotonicitySettings::isMonotoneParametersSet() const {
    return this->getOption(monotoneParameters).getHasOptionBeenSet();
}

std::string MonotonicitySettings::getDotOutputFilename() const {
    return this->getOption(dotOutput).getArgumentByName("dotFilename").getValueAsString();
}

std::string MonotonicitySettings::getMonotoneParameterFilename() const {
    return this->getOption(monotoneParameters).getArgumentByName("monotoneParametersFilename").getValueAsString();
}

uint_fast64_t MonotonicitySettings::getNumberOfSamples() const {
    return this->getOption(samplesMonotonicityAnalysis).getArgumentByName("samples").getValueAsUnsignedInteger();
}

bool MonotonicitySettings::isExportMonotonicitySet() const {
    return this->getOption(exportMonotonicityName).getHasOptionBeenSet();
}

std::string MonotonicitySettings::getExportMonotonicityFilename() const {
    return this->getOption(exportMonotonicityName).getArgumentByName("filename").getValueAsString();
}

uint_fast64_t MonotonicitySettings::getMonotonicityThreshold() const {
    return this->getOption(monotonicityThreshold).getArgumentByName("depth").getValueAsUnsignedInteger();
}

bool MonotonicitySettings::isMonSolutionSet() const {
    return this->getOption(monSolution).getHasOptionBeenSet();
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
