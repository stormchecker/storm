#include "storm/settings/modules/CuddSettings.h"

#include "storm/settings/SettingsManager.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string CuddSettings::moduleName = "cudd";
const std::string CuddSettings::precisionOptionName = "precision";
const std::string CuddSettings::maximalMemoryOptionName = "maxmem";
const std::string CuddSettings::reorderOptionName = "dynreorder";
const std::string CuddSettings::reorderTechniqueOptionName = "reordertechnique";

CuddSettings::CuddSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, true, "Sets the precision used by Cudd.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                                         "value", "The precision up to which to constants are considered to be different.")
                                         .setDefaultValueDouble(1e-15)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, maximalMemoryOptionName, true, "Sets the upper bound of memory available to Cudd in MB.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("value", "The memory available to Cudd (0 means unlimited).")
                             .setDefaultValueUnsignedInteger(4096)
                             .build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, reorderOptionName, false, "Sets whether dynamic reordering is allowed.").setIsAdvanced().build());

    std::vector<std::string> reorderingTechniques;
    reorderingTechniques.push_back("none");
    reorderingTechniques.push_back("random");
    reorderingTechniques.push_back("randompivot");
    reorderingTechniques.push_back("sift");
    reorderingTechniques.push_back("siftconv");
    reorderingTechniques.push_back("ssift");
    reorderingTechniques.push_back("ssiftconv");
    reorderingTechniques.push_back("gsift");
    reorderingTechniques.push_back("gsiftconv");
    reorderingTechniques.push_back("win2");
    reorderingTechniques.push_back("win2conv");
    reorderingTechniques.push_back("win3");
    reorderingTechniques.push_back("win3conv");
    reorderingTechniques.push_back("win4");
    reorderingTechniques.push_back("win4conv");
    reorderingTechniques.push_back("annealing");
    reorderingTechniques.push_back("genetic");
    reorderingTechniques.push_back("exact");
    this->addOption(
        storm::settings::OptionBuilder(moduleName, reorderTechniqueOptionName, true, "Sets the reordering technique used by Cudd.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("method", "Sets which technique is used by Cudd's reordering routines.")
                             .setDefaultValueString("gsift")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(reorderingTechniques))
                             .build())
            .build());
}

double CuddSettings::getConstantPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

uint_fast64_t CuddSettings::getMaximalMemory() const {
    return this->getOption(maximalMemoryOptionName).getArgumentByName("value").getValueAsUnsignedInteger();
}

bool CuddSettings::isReorderingEnabled() const {
    return this->getOption(reorderOptionName).getHasOptionBeenSet();
}

storm::dd::CuddReorderingTechnique CuddSettings::getReorderingTechnique() const {
    std::string reorderingTechniqueAsString = this->getOption(reorderTechniqueOptionName).getArgumentByName("method").getValueAsString();
    if (reorderingTechniqueAsString == "none") {
        return storm::dd::CuddReorderingTechnique::None;
    } else if (reorderingTechniqueAsString == "random") {
        return storm::dd::CuddReorderingTechnique::Random;
    } else if (reorderingTechniqueAsString == "randompivot") {
        return storm::dd::CuddReorderingTechnique::RandomPivot;
    } else if (reorderingTechniqueAsString == "sift") {
        return storm::dd::CuddReorderingTechnique::Sift;
    } else if (reorderingTechniqueAsString == "siftconv") {
        return storm::dd::CuddReorderingTechnique::SiftConv;
    } else if (reorderingTechniqueAsString == "ssift") {
        return storm::dd::CuddReorderingTechnique::SymmetricSift;
    } else if (reorderingTechniqueAsString == "ssiftconv") {
        return storm::dd::CuddReorderingTechnique::SymmetricSiftConv;
    } else if (reorderingTechniqueAsString == "gsift") {
        return storm::dd::CuddReorderingTechnique::GroupSift;
    } else if (reorderingTechniqueAsString == "gsiftconv") {
        return storm::dd::CuddReorderingTechnique::GroupSiftConv;
    } else if (reorderingTechniqueAsString == "win2") {
        return storm::dd::CuddReorderingTechnique::Win2;
    } else if (reorderingTechniqueAsString == "win2conv") {
        return storm::dd::CuddReorderingTechnique::Win2Conv;
    } else if (reorderingTechniqueAsString == "win3") {
        return storm::dd::CuddReorderingTechnique::Win3;
    } else if (reorderingTechniqueAsString == "win3conv") {
        return storm::dd::CuddReorderingTechnique::Win3Conv;
    } else if (reorderingTechniqueAsString == "win4") {
        return storm::dd::CuddReorderingTechnique::Win4;
    } else if (reorderingTechniqueAsString == "win4conv") {
        return storm::dd::CuddReorderingTechnique::Win4Conv;
    } else if (reorderingTechniqueAsString == "annealing") {
        return storm::dd::CuddReorderingTechnique::Annealing;
    } else if (reorderingTechniqueAsString == "genetic") {
        return storm::dd::CuddReorderingTechnique::Genetic;
    } else if (reorderingTechniqueAsString == "exact") {
        return storm::dd::CuddReorderingTechnique::Exact;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Illegal value '" << reorderingTechniqueAsString << "' set as reordering technique of Cudd.");
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
