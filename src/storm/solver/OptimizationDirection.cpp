#include "OptimizationDirection.h"

#include <iostream>

#include "storm/utility/macros.h"

namespace storm {
namespace solver {

bool isSet(OptimizationDirectionSetting s) {
    return s != OptimizationDirectionSetting::Unset;
}

OptimizationDirection convert(OptimizationDirectionSetting s) {
    STORM_LOG_ASSERT(isSet(s), "Setting is not set.");
    return static_cast<OptimizationDirection>(s);
}

OptimizationDirectionSetting convert(OptimizationDirection d) {
    return static_cast<OptimizationDirectionSetting>(d);
}

std::ostream& operator<<(std::ostream& out, OptimizationDirection d) {
    return d == OptimizationDirection::Minimize ? out << "minimize" : out << "maximize";
}
}  // namespace solver
}  // namespace storm
