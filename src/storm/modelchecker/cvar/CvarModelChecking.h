#pragma once

#include <functional>
#include <memory>

#include "storm/logic/CvarFormula.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/storage/BitVector.h"

namespace storm {

class Environment;

namespace logic {
class Formula;
}

namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> performCvarModelChecking(
    Environment const& env, SparseMdpModelType const& model,
    CheckTask<storm::logic::CvarFormula, typename SparseMdpModelType::ValueType> const& checkTask,
    std::function<storm::storage::BitVector(storm::logic::Formula const&)> const& formulaChecker);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
