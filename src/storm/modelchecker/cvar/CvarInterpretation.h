#pragma once

#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

enum class CvarInterpretation { Cost, Reward };

enum class CvarInterpretationSelection { Auto, Cost, Reward };

inline CvarInterpretation resolveCvarInterpretation(CvarInterpretationSelection selection, storm::solver::OptimizationDirection optimizationDirection) {
    switch (selection) {
        case CvarInterpretationSelection::Auto:
            return storm::solver::minimize(optimizationDirection) ? CvarInterpretation::Cost : CvarInterpretation::Reward;
        case CvarInterpretationSelection::Cost:
            return CvarInterpretation::Cost;
        case CvarInterpretationSelection::Reward:
            return CvarInterpretation::Reward;
    }
    STORM_LOG_ASSERT(false, "Encountered an unknown CVaR interpretation selection.");
    return CvarInterpretation::Cost;
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
