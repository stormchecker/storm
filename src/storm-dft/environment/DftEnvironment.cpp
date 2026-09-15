#include "DftEnvironment.h"

#include "storm-dft/environment/AnalysisEnvironment.h"
#include "storm-dft/environment/ModelBuilderEnvironment.h"
#include "storm-dft/environment/TransformationEnvironment.h"
#include "storm/environment/Environment.h"

namespace storm::dft {

DftEnvironment::DftEnvironment() {
    // Intentionally left empty
}

DftEnvironment::~DftEnvironment() {
    // Intentionally left empty.
}

DftEnvironment::DftEnvironment(DftEnvironment const& other) : internalEnv(other.internalEnv) {
    // Intentionally left empty.
}

DftEnvironment& DftEnvironment::operator=(DftEnvironment const& other) {
    internalEnv = other.internalEnv;
    return *this;
}

storm::Environment& DftEnvironment::core() {
    return internalEnv.get().coreEnvironment.get();
}

storm::Environment const& DftEnvironment::core() const {
    return internalEnv.get().coreEnvironment.get();
}

AnalysisEnvironment& DftEnvironment::analysis() {
    return internalEnv.get().analysisEnvironment.get();
}

AnalysisEnvironment const& DftEnvironment::analysis() const {
    return internalEnv.get().analysisEnvironment.get();
}

ModelBuilderEnvironment& DftEnvironment::modelBuilder() {
    return internalEnv.get().modelBuilderEnvironment.get();
}

ModelBuilderEnvironment const& DftEnvironment::modelBuilder() const {
    return internalEnv.get().modelBuilderEnvironment.get();
}

TransformationEnvironment& DftEnvironment::transformation() {
    return internalEnv.get().transformationEnvironment.get();
}

TransformationEnvironment const& DftEnvironment::transformation() const {
    return internalEnv.get().transformationEnvironment.get();
}

}  // namespace storm::dft
