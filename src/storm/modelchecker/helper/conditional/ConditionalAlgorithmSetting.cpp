#include "ConditionalAlgorithmSetting.h"

namespace storm {
std::ostream& operator<<(std::ostream& stream, ConditionalAlgorithmSetting const& algorithm) {
    switch (algorithm) {
        case ConditionalAlgorithmSetting::Default:
            return stream << "default";
        case ConditionalAlgorithmSetting::Restart:
            return stream << "restart";
        case ConditionalAlgorithmSetting::Bisection:
            return stream << "bisection";
        case ConditionalAlgorithmSetting::BisectionAdvanced:
            return stream << "bisection-advanced";
        case ConditionalAlgorithmSetting::BisectionPolicyTracking:
            return stream << "bisection-pt";
        case ConditionalAlgorithmSetting::BisectionAdvancedPolicyTracking:
            return stream << "bisection-advanced-pt";
        case ConditionalAlgorithmSetting::PolicyIteration:
            return stream << "pi";
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unknown conditional algorithm");
    return stream;
}

ConditionalAlgorithmSetting conditionalAlgorithmSettingFromString(std::string const& algorithm) {
    if (algorithm == "default") {
        return ConditionalAlgorithmSetting::Default;
    } else if (algorithm == "restart") {
        return ConditionalAlgorithmSetting::Restart;
    } else if (algorithm == "bisection") {
        return ConditionalAlgorithmSetting::Bisection;
    } else if (algorithm == "bisection-advanced") {
        return ConditionalAlgorithmSetting::BisectionAdvanced;
    } else if (algorithm == "bisection-pt") {
        return ConditionalAlgorithmSetting::BisectionPolicyTracking;
    } else if (algorithm == "bisection-advanced-pt") {
        return ConditionalAlgorithmSetting::BisectionAdvancedPolicyTracking;
    } else if (algorithm == "pi") {
        return ConditionalAlgorithmSetting::PolicyIteration;
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unknown conditional algorithm: " << algorithm);
}

}  // namespace storm