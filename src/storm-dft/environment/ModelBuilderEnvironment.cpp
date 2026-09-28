#include "ModelBuilderEnvironment.h"

#include "storm/utility/macros.h"

namespace storm::dft {

ModelBuilderEnvironment::ModelBuilderEnvironment()
    : useSymmetryReduction(true), allowDCForRelevantEvents(false), addLabelsClaiming(false), takeFirstDependency(false), uniqueFailedBE(false) {
    // Intentionally left empty
}

ModelBuilderEnvironment::~ModelBuilderEnvironment() = default;

bool ModelBuilderEnvironment::isUseSymmetryReduction() const {
    return useSymmetryReduction;
}

void ModelBuilderEnvironment::setUseSymmetryReduction(bool value) {
    useSymmetryReduction = value;
}

bool ModelBuilderEnvironment::isAllowDCForRelevantEvents() const {
    return allowDCForRelevantEvents;
}

void ModelBuilderEnvironment::setAllowDCForRelevantEvents(bool value) {
    allowDCForRelevantEvents = value;
}

bool ModelBuilderEnvironment::isAddLabelsClaiming() const {
    return addLabelsClaiming;
}

void ModelBuilderEnvironment::setAddLabelsClaiming(bool value) {
    addLabelsClaiming = value;
}

bool ModelBuilderEnvironment::isMaxDepthSet() const {
    return maxDepth.has_value();
}

uint_fast64_t ModelBuilderEnvironment::getMaxDepth() const {
    STORM_LOG_ASSERT(isMaxDepthSet(), "MaxDepth is not set.");
    return maxDepth.value();
}

void ModelBuilderEnvironment::setMaxDepth(uint_fast64_t value) {
    maxDepth = value;
}

void ModelBuilderEnvironment::unsetMaxDepth() {
    maxDepth = std::nullopt;
}

bool ModelBuilderEnvironment::isTakeFirstDependency() const {
    return takeFirstDependency;
}

void ModelBuilderEnvironment::setTakeFirstDependency(bool value) {
    takeFirstDependency = value;
}

bool ModelBuilderEnvironment::isUniqueFailedBE() const {
    return uniqueFailedBE;
}

void ModelBuilderEnvironment::setUniqueFailedBE(bool value) {
    uniqueFailedBE = value;
}

}  // namespace storm::dft
