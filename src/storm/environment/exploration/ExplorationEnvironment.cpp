#include "storm/environment/exploration/ExplorationEnvironment.h"

namespace storm {

ExplorationEnvironment::ExplorationEnvironment()
    : precomputationType(storm::modelchecker::exploration_detail::PrecomputationType::Global),
      stepsUntilPrecomputation(100000),
      sampledPathsUntilPrecomputation(std::nullopt),
      nextStateHeuristic(storm::modelchecker::exploration_detail::NextStateHeuristic::DifferenceProbabilitySum),
      precision(1e-06) {
    // Intentionally left empty.
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

std::optional<uint64_t> const& ExplorationEnvironment::getSampledPathsUntilPrecomputation() const {
    return sampledPathsUntilPrecomputation;
}

void ExplorationEnvironment::setSampledPathsUntilPrecomputation(uint64_t value) {
    sampledPathsUntilPrecomputation = value;
}

void ExplorationEnvironment::unsetSampledPathsUntilPrecomputation() {
    sampledPathsUntilPrecomputation = std::nullopt;
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
