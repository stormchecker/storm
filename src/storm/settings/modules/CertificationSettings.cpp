#include "storm/settings/modules/CertificationSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm::settings::modules {

const std::string CertificationSettings::moduleName = "cert";

const std::string CertificationSettings::produceCertificateOptionName = "certificate";
const std::string CertificationSettings::useTopologicalOptionName = "topological";
const std::string CertificationSettings::methodOptionName = "method";

CertificationSettings::CertificationSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, produceCertificateOptionName, false, "If set, a certificate will be produced (if supported).").build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, useTopologicalOptionName, false, "If set, uses topological methods to produce certificates.").build());
    std::vector<std::string> const methodNames = {"fp-ii", "fp-smoothii", "fp-roundii", "ex-pi"};
    this->addOption(storm::settings::OptionBuilder(moduleName, methodOptionName, true, "The method to use for certification.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .setDefaultValueString("fp-ii")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methodNames))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("gamma", "The gamma value for the method.")
                                         .setDefaultValueDouble(0.0)
                                         .makeOptional()
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("delta", "The delta value for the method.")
                                         .setDefaultValueDouble(0.0)
                                         .makeOptional()
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .build());
}

bool CertificationSettings::isProduceCertificateSet() const {
    return this->getOption(produceCertificateOptionName).getHasOptionBeenSet();
}

bool CertificationSettings::isUseTopologicalSet() const {
    return this->getOption(useTopologicalOptionName).getHasOptionBeenSet();
}

storm::CertificateAlgorithm CertificationSettings::getAlgorithm() const {
    return storm::certificateAlgorithmFromString(this->getOption(methodOptionName).getArgumentByName("name").getValueAsString());
}

double CertificationSettings::getGamma() const {
    return this->getOption(methodOptionName).getArgumentByName("gamma").getValueAsDouble();
}

double CertificationSettings::getDelta() const {
    return this->getOption(methodOptionName).getArgumentByName("delta").getValueAsDouble();
}

bool CertificationSettings::check() const {
    if (this->getOption(methodOptionName).getHasOptionBeenSet()) {
        auto const algorithm = getAlgorithm();
        if (algorithm == storm::CertificateAlgorithm::FpSmoothII || algorithm == storm::CertificateAlgorithm::FpRoundII) {
            // For gamma = 1, the bounds are never updated, so the iteration would not terminate.
            STORM_LOG_THROW(getGamma() < 1.0, storm::exceptions::InvalidSettingsException,
                            "The parameter gamma for smooth and rounding interval iteration must be smaller than 1.");
        }
    }
    return true;
}

}  // namespace storm::settings::modules
