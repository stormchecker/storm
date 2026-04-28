#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"

#include "storm/environment/modelchecker/CvarModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {

ModelCheckerEnvironment::ModelCheckerEnvironment() {
    auto const& mcSettings = storm::settings::getModule<storm::settings::modules::ModelCheckerSettings>();
    if (mcSettings.isLtl2daToolSet()) {
        ltl2daTool = mcSettings.getLtl2daTool();
    }
    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    steadyStateDistributionAlgorithm = ioSettings.getSteadyStateDistributionAlgorithm();

    conditionalAlgorithmSetting = mcSettings.getConditionalAlgorithmSetting();
}

ModelCheckerEnvironment::~ModelCheckerEnvironment() {
    // Intentionally left empty
}

CvarModelCheckerEnvironment& ModelCheckerEnvironment::cvar() {
    return cvarModelCheckerEnvironment.get();
}

CvarModelCheckerEnvironment const& ModelCheckerEnvironment::cvar() const {
    return cvarModelCheckerEnvironment.get();
}

SteadyStateDistributionAlgorithm ModelCheckerEnvironment::getSteadyStateDistributionAlgorithm() const {
    return steadyStateDistributionAlgorithm;
}

void ModelCheckerEnvironment::setSteadyStateDistributionAlgorithm(SteadyStateDistributionAlgorithm value) {
    steadyStateDistributionAlgorithm = value;
}

ConditionalAlgorithmSetting ModelCheckerEnvironment::getConditionalAlgorithmSetting() const {
    return conditionalAlgorithmSetting;
}

void ModelCheckerEnvironment::setConditionalAlgorithmSetting(ConditionalAlgorithmSetting value) {
    conditionalAlgorithmSetting = value;
}

MultiObjectiveModelCheckerEnvironment& ModelCheckerEnvironment::multi() {
    return multiObjectiveModelCheckerEnvironment.get();
}

MultiObjectiveModelCheckerEnvironment const& ModelCheckerEnvironment::multi() const {
    return multiObjectiveModelCheckerEnvironment.get();
}

bool ModelCheckerEnvironment::isLtl2daToolSet() const {
    return ltl2daTool.is_initialized();
}

std::string const& ModelCheckerEnvironment::getLtl2daTool() const {
    return ltl2daTool.get();
}

void ModelCheckerEnvironment::setLtl2daTool(std::string const& value) {
    ltl2daTool = value;
}

void ModelCheckerEnvironment::unsetLtl2daTool() {
    ltl2daTool = boost::none;
}

}  // namespace storm
