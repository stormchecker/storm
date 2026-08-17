#pragma once

#include <iostream>
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
// An enumeration of all resolution modes to resolve the uncertainty (e.g. intervals) by nature.
enum class UncertaintyResolutionMode { Minimize, Maximize, Robust, Cooperative, Unset };

// An enumeration of all resolution modes, available through the CLI, to resolve the uncertainty (e.g. intervals) by nature.
enum class UncertaintyResolutionModeSetting { Minimize, Maximize, Robust, Cooperative, Both };

std::ostream& operator<<(std::ostream& out, UncertaintyResolutionMode mode);
bool isSet(UncertaintyResolutionMode uncertaintyResolutionMode);
bool isUncertaintyResolvedRobust(UncertaintyResolutionMode uncertaintyResolutionMode, OptimizationDirection optimizationDirection);
UncertaintyResolutionMode convert(UncertaintyResolutionModeSetting uncertaintyResolutionModeSetting);

}  // namespace solver

using UncertaintyResolutionModeSetting = solver::UncertaintyResolutionModeSetting;
using UncertaintyResolutionMode = solver::UncertaintyResolutionMode;
}  // namespace storm
