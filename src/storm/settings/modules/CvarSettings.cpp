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

CvarSettings::CvarSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"auto", "wr", "weighted-reachability", "ssp"};
    this->addOption(storm::settings::OptionBuilder(moduleName, methodOptionName, true, "The method to be used for CVaR model checking.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("auto")
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

}  // namespace modules
}  // namespace settings
}  // namespace storm
