#include "ExplicitQualitativeResultMinMax.h"

namespace storm::gbar {
namespace abstraction {

bool ExplicitQualitativeResultMinMax::isExplicit() const {
    return true;
}

ExplicitQualitativeResult const& ExplicitQualitativeResultMinMax::getProb0Min() const {
    return getProb0(storm::OptimizationDirection::Minimize);
}

ExplicitQualitativeResult const& ExplicitQualitativeResultMinMax::getProb1Min() const {
    return getProb1(storm::OptimizationDirection::Minimize);
}

ExplicitQualitativeResult const& ExplicitQualitativeResultMinMax::getProb0Max() const {
    return getProb0(storm::OptimizationDirection::Maximize);
}

ExplicitQualitativeResult const& ExplicitQualitativeResultMinMax::getProb1Max() const {
    return getProb1(storm::OptimizationDirection::Maximize);
}

ExplicitQualitativeResult& ExplicitQualitativeResultMinMax::getProb0Min() {
    return getProb0(storm::OptimizationDirection::Minimize);
}

ExplicitQualitativeResult& ExplicitQualitativeResultMinMax::getProb1Min() {
    return getProb1(storm::OptimizationDirection::Minimize);
}

ExplicitQualitativeResult& ExplicitQualitativeResultMinMax::getProb0Max() {
    return getProb0(storm::OptimizationDirection::Maximize);
}

ExplicitQualitativeResult& ExplicitQualitativeResultMinMax::getProb1Max() {
    return getProb1(storm::OptimizationDirection::Maximize);
}

}  // namespace abstraction
}  // namespace storm::gbar
