#include "storm-pars/settings/modules/SamplingSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm::settings::modules {

const std::string SamplingSettings::moduleName = "sampling";
const std::string samplesOptionName = "samples";
const std::string samplesGraphPreservingOptionName = "samples-graph-preserving";
const std::string sampleExactOptionName = "sample-exact";
const std::string sampleDerivativeOptionName = "sample-derivative";

SamplingSettings::SamplingSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, samplesOptionName, false, "The points at which to sample the model.")
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument(
                    "samples", "The samples are semicolon-separated entries of the form 'Var1=Val1:Val2:...:Valk,Var2=... that span the sample spaces.")
                    .setDefaultValueString("")
                    .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, samplesGraphPreservingOptionName, false,
                                                   "Sets whether it can be assumed that the samples are graph-preserving.")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, sampleExactOptionName, false, "Sets whether to sample using exact arithmetic.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, sampleDerivativeOptionName, false, "Sets whether to sample the derivatives.").build());
}

std::string SamplingSettings::getSamples() const {
    return this->getOption(samplesOptionName).getArgumentByName("samples").getValueAsString();
}

bool SamplingSettings::isSamplesAreGraphPreservingSet() const {
    return this->getOption(samplesGraphPreservingOptionName).getHasOptionBeenSet();
}

bool SamplingSettings::isSampleExactSet() const {
    return this->getOption(sampleExactOptionName).getHasOptionBeenSet();
}

bool SamplingSettings::isSampleDerivativeSet() const {
    return this->getOption(sampleDerivativeOptionName).getHasOptionBeenSet();
}
}  // namespace storm::settings::modules