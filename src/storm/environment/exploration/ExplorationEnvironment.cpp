#include "storm/environment/exploration/ExplorationEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ExplorationSettings.h"

namespace storm {

ExplorationEnvironment::ExplorationEnvironment() {
    auto const& explSettings = storm::settings::getModule<storm::settings::modules::ExplorationSettings>();
    precomputationType = explSettings.getPrecomputationType();
    stepsUntilPrecomputation = static_cast<uint64_t>(explSettings.getNumberOfExplorationStepsUntilPrecomputation());
    if (explSettings.isNumberOfSampledPathsUntilPrecomputationSet()) {
        sampledPathsUntilPrecomputation = explSettings.getNumberOfSampledPathsUntilPrecomputation();
    }
    nextStateHeuristic = explSettings.getNextStateHeuristic();
    precision = explSettings.getPrecision();
}

ExplorationEnvironment::~ExplorationEnvironment() {
    // Intentionally left empty.
}

ExplorationEnvironment::ExplorationEnvironment(ExplorationEnvironment const& other) = default;

ExplorationEnvironment& ExplorationEnvironment::operator=(ExplorationEnvironment const& other) = default;

storm::modelchecker::exploration_detail::PrecomputationType ExplorationEnvironment::getPrecomputationType() const {
    return precomputationType;
}

void ExplorationEnvironment::setPrecomputationType(storm::modelchecker::exploration_detail::PrecomputationType value) {
    precomputationType = value;
}

uint64_t ExplorationEnvironment::getStepsUntilPrecomputation() const {
    return stepsUntilPrecomputation;
}

void ExplorationEnvironment::setStepsUntilPrecomputation(uint64_t value) {
    stepsUntilPrecomputation = value;
}

boost::optional<uint64_t> const& ExplorationEnvironment::getSampledPathsUntilPrecomputation() const {
    return sampledPathsUntilPrecomputation;
}

void ExplorationEnvironment::setSampledPathsUntilPrecomputation(uint64_t value) {
    sampledPathsUntilPrecomputation = value;
}

void ExplorationEnvironment::unsetSampledPathsUntilPrecomputation() {
    sampledPathsUntilPrecomputation = boost::none;
}

storm::modelchecker::exploration_detail::NextStateHeuristic ExplorationEnvironment::getNextStateHeuristic() const {
    return nextStateHeuristic;
}

void ExplorationEnvironment::setNextStateHeuristic(storm::modelchecker::exploration_detail::NextStateHeuristic value) {
    nextStateHeuristic = value;
}

double ExplorationEnvironment::getPrecision() const {
    return precision;
}

void ExplorationEnvironment::setPrecision(double value) {
    precision = value;
}

}  // namespace storm
