#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"

namespace storm {
namespace settings {
namespace modules {

const std::string BisimulationSettings::moduleName = "bisimulation";
const std::string BisimulationSettings::typeOptionName = "type";
const std::string BisimulationSettings::representativeOptionName = "repr";
const std::string BisimulationSettings::originalVariablesOptionName = "origvars";
const std::string BisimulationSettings::quotientFormatOptionName = "quot";
const std::string BisimulationSettings::signatureModeOptionName = "sigmode";
const std::string BisimulationSettings::reuseOptionName = "reuse";
const std::string BisimulationSettings::initialPartitionOptionName = "init";
const std::string BisimulationSettings::refinementModeOptionName = "refine";
const std::string BisimulationSettings::exactArithmeticDdOptionName = "ddexact";

BisimulationSettings::BisimulationSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> types = {"strong", "weak"};
    this->addOption(storm::settings::OptionBuilder(moduleName, typeOptionName, true, "Sets the kind of bisimulation quotienting used.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the type to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(types))
                                         .setDefaultValueString("strong")
                                         .build())
                        .build());

    std::vector<std::string> quotTypes = {"sparse", "dd"};
    this->addOption(storm::settings::OptionBuilder(moduleName, quotientFormatOptionName, true,
                                                   "Sets the format in which the quotient is extracted (only applies to DD-based bisimulation).")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("format", "The format of the quotient.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(quotTypes))
                                         .setDefaultValueString("dd")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, representativeOptionName, false,
                                                   "Sets whether to use representatives in the quotient rather than block numbers.")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, originalVariablesOptionName, false,
                                                   "Sets whether to use the original variables in the quotient rather than the block variables.")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exactArithmeticDdOptionName, false, "Sets whether to use exact arithmetic in dd-based bisimulation.")
            .setIsAdvanced()
            .build());

    std::vector<std::string> signatureModes = {"eager", "lazy"};
    this->addOption(storm::settings::OptionBuilder(moduleName, signatureModeOptionName, false, "Sets the signature computation mode.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(signatureModes))
                                         .setDefaultValueString("eager")
                                         .build())
                        .build());

    std::vector<std::string> reuseModes = {"none", "blocks"};
    this->addOption(storm::settings::OptionBuilder(moduleName, reuseOptionName, true, "Sets whether to reuse all results.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(reuseModes))
                                         .setDefaultValueString("blocks")
                                         .build())
                        .build());

    std::vector<std::string> initialPartitionModes = {"regular", "finer"};
    this->addOption(storm::settings::OptionBuilder(moduleName, initialPartitionOptionName, true, "Sets which initial partition mode to use.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(initialPartitionModes))
                                         .setDefaultValueString("finer")
                                         .build())
                        .build());

    std::vector<std::string> refinementModes = {"full", "changed"};
    this->addOption(storm::settings::OptionBuilder(moduleName, refinementModeOptionName, true, "Sets which refinement mode to use.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(refinementModes))
                                         .setDefaultValueString("full")
                                         .build())
                        .build());
}

bool BisimulationSettings::isStrongBisimulationSet() const {
    if (this->getOption(typeOptionName).getArgumentByName("name").getValueAsString() == "strong") {
        return true;
    }
    return false;
}

bool BisimulationSettings::isWeakBisimulationSet() const {
    if (this->getOption(typeOptionName).getArgumentByName("name").getValueAsString() == "weak") {
        return true;
    }
    return false;
}

bool BisimulationSettings::isQuotientFormatSetFromDefaultValue() const {
    return !this->getOption(quotientFormatOptionName).getHasOptionBeenSet() ||
           this->getOption(quotientFormatOptionName).getArgumentByName("format").wasSetFromDefaultValue();
}

storm::dd::bisimulation::QuotientFormat BisimulationSettings::getQuotientFormat() const {
    std::string quotientFormatAsString = this->getOption(quotientFormatOptionName).getArgumentByName("format").getValueAsString();
    if (quotientFormatAsString == "sparse") {
        return storm::dd::bisimulation::QuotientFormat::Sparse;
    }
    STORM_LOG_ASSERT(quotientFormatAsString == "dd", "Invalid bisimulation quotient format: " << quotientFormatAsString << ".");
    return storm::dd::bisimulation::QuotientFormat::Dd;
}

bool BisimulationSettings::isUseRepresentativesSet() const {
    return this->getOption(representativeOptionName).getHasOptionBeenSet();
}

bool BisimulationSettings::isUseOriginalVariablesSet() const {
    return this->getOption(originalVariablesOptionName).getHasOptionBeenSet();
}

bool BisimulationSettings::useExactArithmeticInDdBisimulation() const {
    return this->getOption(exactArithmeticDdOptionName).getHasOptionBeenSet();
}

storm::dd::bisimulation::SignatureMode BisimulationSettings::getSignatureMode() const {
    std::string modeAsString = this->getOption(signatureModeOptionName).getArgumentByName("mode").getValueAsString();
    if (modeAsString == "eager") {
        return storm::dd::bisimulation::SignatureMode::Eager;
    } else if (modeAsString == "lazy") {
        return storm::dd::bisimulation::SignatureMode::Lazy;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "Unknown signature mode '" << modeAsString << ".");
}

storm::dd::bisimulation::ReuseMode BisimulationSettings::getReuseMode() const {
    std::string reuseModeAsString = this->getOption(reuseOptionName).getArgumentByName("mode").getValueAsString();
    if (reuseModeAsString == "none") {
        return storm::dd::bisimulation::ReuseMode::None;
    } else if (reuseModeAsString == "blocks") {
        return storm::dd::bisimulation::ReuseMode::BlockNumbers;
    }
    return storm::dd::bisimulation::ReuseMode::BlockNumbers;
}

storm::dd::bisimulation::InitialPartitionMode BisimulationSettings::getInitialPartitionMode() const {
    std::string initialPartitionModeAsString = this->getOption(initialPartitionOptionName).getArgumentByName("mode").getValueAsString();
    if (initialPartitionModeAsString == "regular") {
        return storm::dd::bisimulation::InitialPartitionMode::Regular;
    } else if (initialPartitionModeAsString == "finer") {
        return storm::dd::bisimulation::InitialPartitionMode::Finer;
    }
    return storm::dd::bisimulation::InitialPartitionMode::Finer;
}

storm::dd::bisimulation::RefinementMode BisimulationSettings::getRefinementMode() const {
    std::string refinementModeAsString = this->getOption(refinementModeOptionName).getArgumentByName("mode").getValueAsString();
    if (refinementModeAsString == "full") {
        return storm::dd::bisimulation::RefinementMode::Full;
    } else if (refinementModeAsString == "changed") {
        return storm::dd::bisimulation::RefinementMode::ChangedStates;
    }
    return storm::dd::bisimulation::RefinementMode::Full;
}

bool BisimulationSettings::check() const {
    bool optionsSet = this->getOption(typeOptionName).getHasOptionBeenSet();
    STORM_LOG_WARN_COND(storm::settings::getModule<storm::settings::modules::GeneralSettings>().isBisimulationSet() || !optionsSet,
                        "Bisimulation minimization is not selected, so setting options for bisimulation has no effect.");
    return true;
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
