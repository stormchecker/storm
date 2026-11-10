#include "AnalysisEnvironment.h"

#include "storm-dft/builder/DftExplorationHeuristic.h"
#include "storm/utility/macros.h"

namespace storm::dft {

AnalysisEnvironment::AnalysisEnvironment()
    : useModularisation(false),
      solveWithSMT(false),
      chunksize(1),
      mttfPrecision(1e-12),
      mttfStepsize(1e-10),
      mttfAlgorithm("proceeding"),
      approximationHeuristic(storm::dft::builder::ApproximationHeuristic::DEPTH) {
    // Intentionally left empty
}

AnalysisEnvironment::~AnalysisEnvironment() = default;

bool AnalysisEnvironment::isUseModularisation() const {
    return useModularisation;
}

void AnalysisEnvironment::setUseModularisation(bool value) {
    useModularisation = value;
}

bool AnalysisEnvironment::isSolveWithSMT() const {
    return solveWithSMT;
}

void AnalysisEnvironment::setSolveWithSMT(bool value) {
    solveWithSMT = value;
}

size_t AnalysisEnvironment::getChunksize() const {
    return chunksize;
}

void AnalysisEnvironment::setChunksize(size_t value) {
    chunksize = value;
}

double AnalysisEnvironment::getMttfPrecision() const {
    return mttfPrecision;
}

void AnalysisEnvironment::setMttfPrecision(double value) {
    mttfPrecision = value;
}

double AnalysisEnvironment::getMttfStepsize() const {
    return mttfStepsize;
}

void AnalysisEnvironment::setMttfStepsize(double value) {
    mttfStepsize = value;
}

std::string const& AnalysisEnvironment::getMttfAlgorithm() const {
    return mttfAlgorithm;
}

void AnalysisEnvironment::setMttfAlgorithm(std::string const& value) {
    mttfAlgorithm = value;
}

bool AnalysisEnvironment::isApproximationErrorSet() const {
    return approximationError.has_value();
}

double AnalysisEnvironment::getApproximationError() const {
    STORM_LOG_ASSERT(isApproximationErrorSet(), "Approximation error is not set.");
    return approximationError.value();
}

void AnalysisEnvironment::setApproximationError(double value) {
    approximationError = value;
}

void AnalysisEnvironment::unsetApproximationError() {
    approximationError = std::nullopt;
}

storm::dft::builder::ApproximationHeuristic AnalysisEnvironment::getApproximationHeuristic() const {
    return approximationHeuristic;
}

void AnalysisEnvironment::setApproximationHeuristic(storm::dft::builder::ApproximationHeuristic value) {
    approximationHeuristic = value;
}

}  // namespace storm::dft
