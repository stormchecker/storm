#include "ParametricMode.h"

#include <string>

namespace storm::pars::utility {
std::optional<ParametricMode> getParametricModeFromString(std::string const& input) {
    std::optional<ParametricMode> result = std::nullopt;
    if (input == "feasibility") {
        result = ParametricMode::Feasibility;
    } else if (input == "verification") {
        result = ParametricMode::Verification;
    } else if (input == "monotonicity") {
        result = ParametricMode::Monotonicity;
    } else if (input == "sampling") {
        result = ParametricMode::Sampling;
    } else if (input == "solutionfunction") {
        result = ParametricMode::SolutionFunction;
    } else if (input == "partitioning") {
        result = ParametricMode::Partitioning;
    }
    return result;
}
}  // namespace storm::pars::utility