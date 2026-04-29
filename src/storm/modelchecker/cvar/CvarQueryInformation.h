#pragma once

#include <memory>
#include <string>

#include "storm/logic/CvarFormula.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {
namespace cvar {

struct CvarQueryInformation {
    double alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    boost::optional<std::string> rewardModelName;
    std::shared_ptr<storm::logic::Formula const> targetFormula;
};

CvarQueryInformation extractCvarQueryInformation(storm::logic::CvarFormula const& formula);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
