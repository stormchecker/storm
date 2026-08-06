#pragma once

#include <iostream>

namespace storm {
namespace solver {
enum class OptimizationDirection { Minimize = 0, Maximize = 1 };

bool constexpr minimize(OptimizationDirection d) {
    return d == OptimizationDirection::Minimize;
}

bool constexpr maximize(OptimizationDirection d) {
    return d == OptimizationDirection::Maximize;
}

OptimizationDirection constexpr invert(OptimizationDirection d) {
    return d == OptimizationDirection::Minimize ? OptimizationDirection::Maximize : OptimizationDirection::Minimize;
}
std::ostream& operator<<(std::ostream& out, OptimizationDirection d);

enum class OptimizationDirectionSetting { Minimize = 0, Maximize = 1, Unset };
bool isSet(OptimizationDirectionSetting s);
OptimizationDirection convert(OptimizationDirectionSetting s);
OptimizationDirectionSetting convert(OptimizationDirection d);

}  // namespace solver

using OptimizationDirection = solver::OptimizationDirection;
}  // namespace storm
