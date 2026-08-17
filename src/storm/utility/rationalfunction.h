#pragma once

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::utility::vector {
inline std::set<storm::RationalFunctionVariable> getVariables(std::vector<storm::RationalFunction> const& vector) {
    std::set<storm::RationalFunctionVariable> result;
    for (auto const& entry : vector) {
        entry.gatherVariables(result);
    }
    return result;
}
}  // namespace storm::utility::vector
