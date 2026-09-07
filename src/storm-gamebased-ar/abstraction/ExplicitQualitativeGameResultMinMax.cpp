#include "ExplicitQualitativeGameResultMinMax.h"

namespace storm::gbar {
namespace abstraction {

ExplicitQualitativeGameResult const& ExplicitQualitativeGameResultMinMax::getProb0(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob0Min;
    } else {
        return prob0Max;
    }
}

ExplicitQualitativeGameResult const& ExplicitQualitativeGameResultMinMax::getProb1(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob1Min;
    } else {
        return prob1Max;
    }
}

ExplicitQualitativeGameResult& ExplicitQualitativeGameResultMinMax::getProb0(storm::OptimizationDirection const& dir) {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob0Min;
    } else {
        return prob0Max;
    }
}

ExplicitQualitativeGameResult& ExplicitQualitativeGameResultMinMax::getProb1(storm::OptimizationDirection const& dir) {
    if (dir == storm::OptimizationDirection::Minimize) {
        return prob1Min;
    } else {
        return prob1Max;
    }
}
}  // namespace abstraction
}  // namespace storm::gbar
