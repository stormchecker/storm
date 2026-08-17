#pragma once
#include <ostream>

#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm {
enum class ConditionalAlgorithmSetting {
    Default,
    Restart,
    Bisection,
    BisectionAdvanced,
    BisectionPolicyTracking,
    BisectionAdvancedPolicyTracking,
    PolicyIteration
};

std::ostream& operator<<(std::ostream& stream, ConditionalAlgorithmSetting const& algorithm);
ConditionalAlgorithmSetting conditionalAlgorithmSettingFromString(std::string const& algorithm);

}  // namespace storm