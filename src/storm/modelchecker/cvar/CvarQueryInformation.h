#pragma once

#include <memory>
#include <optional>
#include <string>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/CvarFormula.h"
#include "storm/modelchecker/cvar/CvarInterpretation.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {
namespace cvar {

struct CvarQueryInformation {
    storm::RationalNumber alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    CvarInterpretation interpretation;
    std::optional<std::string> rewardModelName;
    std::shared_ptr<storm::logic::Formula const> targetFormula;
};

CvarQueryInformation extractCvarQueryInformation(storm::logic::CvarFormula const& formula, CvarInterpretation interpretation);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
