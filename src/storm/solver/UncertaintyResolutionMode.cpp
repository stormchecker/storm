#include "UncertaintyResolutionMode.h"

namespace storm {
namespace solver {
std::ostream& operator<<(std::ostream& out, UncertaintyResolutionMode mode) {
    switch (mode) {
        case UncertaintyResolutionMode::Minimize:
            out << "minimize";
            break;
        case UncertaintyResolutionMode::Maximize:
            out << "maximize";
            break;
        case UncertaintyResolutionMode::Robust:
            out << "robust";
            break;
        case UncertaintyResolutionMode::Cooperative:
            out << "cooperative";
            break;
        case UncertaintyResolutionMode::Unset:
            out << "unset";
            break;
    }
    return out;
}

bool isSet(UncertaintyResolutionMode uncertaintyResolutionMode) {
    return uncertaintyResolutionMode != UncertaintyResolutionMode::Unset;
}

bool isUncertaintyResolvedRobust(UncertaintyResolutionMode uncertaintyResolutionMode, OptimizationDirection optimizationDirection) {
    switch (uncertaintyResolutionMode) {
        case UncertaintyResolutionMode::Maximize:
            return optimizationDirection != OptimizationDirection::Maximize;
        case UncertaintyResolutionMode::Minimize:
            return optimizationDirection != OptimizationDirection::Minimize;
        case UncertaintyResolutionMode::Robust:
            return true;
        case UncertaintyResolutionMode::Cooperative:
            return false;
        case UncertaintyResolutionMode::Unset:
            STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "Uncertainty resolution mode not set properly.");
    }

    return true;
}

UncertaintyResolutionMode convert(UncertaintyResolutionModeSetting settingMode) {
    STORM_LOG_THROW(settingMode != UncertaintyResolutionModeSetting::Both, storm::exceptions::InvalidSettingsException,
                    "Cannot convert uncertainty resolution setting mode 'both'.");
    return static_cast<UncertaintyResolutionMode>(settingMode);
}

}  // namespace solver
}  // namespace storm