#include "storm/settings/modules/CvarSettings.h"

#include <vector>

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

std::string const CvarSettings::moduleName = "cvar";
std::string const CvarSettings::methodOptionName = "method";
std::string const CvarSettings::interpretationOptionName = "interpretation";
std::string const CvarSettings::zeroWeightTransformationOptionName = "zero-weight-transformation";

CvarSettings::CvarSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"auto", "wr", "weighted-reachability", "ssp"};
    std::vector<std::string> interpretations = {"auto", "cost", "reward"};
    std::vector<std::string> zeroWeightTransformations = {"disabled", "global-collapse", "local-elimination", "on-the-fly-lp"};
    this->addOption(storm::settings::OptionBuilder(moduleName, methodOptionName, true, "The method to be used for CVaR model checking.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("auto")
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, interpretationOptionName, true, "The interpretation to be used for CVaR model checking.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the interpretation to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(interpretations))
                                         .setDefaultValueString("auto")
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, zeroWeightTransformationOptionName, true, "The method for zero-weight choices in CVaR SSP models.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The method to use.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(zeroWeightTransformations))
                             .setDefaultValueString("disabled")
                             .build())
            .build());
}

storm::modelchecker::cvar::CvarMethod CvarSettings::getCvarMethod() const {
    std::string methodAsString = this->getOption(methodOptionName).getArgumentByName("name").getValueAsString();
    if (methodAsString == "auto") {
        return storm::modelchecker::cvar::CvarMethod::Auto;
    } else if (methodAsString == "wr" || methodAsString == "weighted-reachability") {
        return storm::modelchecker::cvar::CvarMethod::WeightedReachability;
    } else if (methodAsString == "ssp" || methodAsString == "ssp-vi" || methodAsString == "pareto-vi") {
        return storm::modelchecker::cvar::CvarMethod::SspParetoVi;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown CVaR method '" << methodAsString << "'.");
}

storm::modelchecker::cvar::CvarInterpretationSelection CvarSettings::getInterpretationSelection() const {
    std::string interpretationAsString = this->getOption(interpretationOptionName).getArgumentByName("name").getValueAsString();
    if (interpretationAsString == "auto") {
        return storm::modelchecker::cvar::CvarInterpretationSelection::Auto;
    } else if (interpretationAsString == "cost") {
        return storm::modelchecker::cvar::CvarInterpretationSelection::Cost;
    } else if (interpretationAsString == "reward") {
        return storm::modelchecker::cvar::CvarInterpretationSelection::Reward;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown CVaR interpretation '" << interpretationAsString << "'.");
}

storm::modelchecker::cvar::ZeroWeightTransformation CvarSettings::getZeroWeightTransformation() const {
    std::string transformation = this->getOption(zeroWeightTransformationOptionName).getArgumentByName("name").getValueAsString();
    if (transformation == "disabled") {
        return storm::modelchecker::cvar::ZeroWeightTransformation::Disabled;
    } else if (transformation == "global-collapse") {
        return storm::modelchecker::cvar::ZeroWeightTransformation::GlobalCollapse;
    } else if (transformation == "local-elimination") {
        return storm::modelchecker::cvar::ZeroWeightTransformation::LocalElimination;
    } else if (transformation == "on-the-fly-lp") {
        return storm::modelchecker::cvar::ZeroWeightTransformation::OnTheFlyLp;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown zero-weight transformation '" << transformation << "'.");
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
