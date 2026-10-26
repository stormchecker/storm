#include "storm-pars/settings/modules/ScpSettings.h"

#include "storm-pars/scp/ScpOptions.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/ArgumentValidators.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm::settings::modules {

const std::string ScpSettings::moduleName = "scp";
namespace {
// The defaults of the options are taken from the library's default-constructed options.
storm::pars::scp::ScpOptions const defaultOptions;

const std::string trustRegionInitialOption = "trust-region-initial";
const std::string trustRegionFactorOption = "trust-region-factor";
const std::string trustRegionMaxOption = "trust-region-max";
const std::string trustRegionMinExcessOption = "trust-region-min-excess";
const std::string maxIterationsOption = "max-iterations";
const std::string initialStateBiasOption = "initial-state-bias";
const std::string exactOption = "exact-checks";
const std::string sumViolationObjectiveOption = "sum-violation-objective";
}  // namespace

ScpSettings::ScpSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, trustRegionInitialOption, false, "The initial trust region radius (delta_0)")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(trustRegionInitialOption, "The initial radius")
                                         .setDefaultValueDouble(defaultOptions.trustRegionInitial)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterValidator(1.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, trustRegionFactorOption, false, "The trust region growth/shrink factor (gamma)")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(trustRegionFactorOption, "The factor")
                                         .setDefaultValueDouble(defaultOptions.trustRegionFactor)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterValidator(1.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, trustRegionMaxOption, false, "The upper cap on the trust region radius")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(trustRegionMaxOption, "The cap")
                                         .setDefaultValueDouble(defaultOptions.trustRegionMax)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterValidator(1.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, trustRegionMinExcessOption, false,
                                                   "The termination threshold on (trust region radius - 1); the radius asymptotically approaches 1, never 0")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(trustRegionMinExcessOption, "The threshold")
                                         .setDefaultValueDouble(defaultOptions.trustRegionMinExcess)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, maxIterationsOption, false, "The maximum number of SCP iterations")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createIntegerArgument(maxIterationsOption, "The maximum number of iterations")
                                         .setDefaultValueInteger(defaultOptions.maxIterations)
                                         .addValidatorInteger(storm::settings::ArgumentValidatorFactory::createIntegerGreaterEqualValidator(1))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, initialStateBiasOption, false,
                                                   "The weight of the initial state's value in the LP objective; it breaks ties towards the threshold side")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(initialStateBiasOption, "The weight")
                                         .setDefaultValueDouble(defaultOptions.initialStateBias)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exactOption, false, "Sets whether candidate valuations are model-checked using exact arithmetic.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, sumViolationObjectiveOption, false,
                                                   "Minimize the sum of per-state constraint violations instead of the default maximum violation")
                        .setIsAdvanced()
                        .build());
}

double ScpSettings::getTrustRegionInitial() const {
    return this->getOption(trustRegionInitialOption).getArgumentByName(trustRegionInitialOption).getValueAsDouble();
}

double ScpSettings::getTrustRegionFactor() const {
    return this->getOption(trustRegionFactorOption).getArgumentByName(trustRegionFactorOption).getValueAsDouble();
}

double ScpSettings::getTrustRegionMax() const {
    return this->getOption(trustRegionMaxOption).getArgumentByName(trustRegionMaxOption).getValueAsDouble();
}

double ScpSettings::getTrustRegionMinExcess() const {
    return this->getOption(trustRegionMinExcessOption).getArgumentByName(trustRegionMinExcessOption).getValueAsDouble();
}

uint64_t ScpSettings::getMaxIterations() const {
    return this->getOption(maxIterationsOption).getArgumentByName(maxIterationsOption).getValueAsInteger();
}

double ScpSettings::getInitialStateBias() const {
    return this->getOption(initialStateBiasOption).getArgumentByName(initialStateBiasOption).getValueAsDouble();
}

bool ScpSettings::isExactSet() const {
    return this->getOption(exactOption).getHasOptionBeenSet();
}

bool ScpSettings::isSumViolationObjectiveSet() const {
    return this->getOption(sumViolationObjectiveOption).getHasOptionBeenSet();
}

}  // namespace storm::settings::modules
