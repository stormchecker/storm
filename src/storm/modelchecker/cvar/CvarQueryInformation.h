#pragma once

#include <memory>
#include <string>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/CvarFormula.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {
namespace cvar {

enum class CvarInterpretation { Cost, Reward };

struct CvarQueryInformation {
    storm::RationalNumber alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    CvarInterpretation interpretation;
    boost::optional<std::string> rewardModelName;
    std::shared_ptr<storm::logic::Formula const> targetFormula;
};

CvarQueryInformation extractCvarQueryInformation(storm::logic::CvarFormula const& formula);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
