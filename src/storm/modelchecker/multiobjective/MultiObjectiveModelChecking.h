#pragma once

#include <memory>

#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/CheckResult.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace multiobjective {

template<typename SparseModelType>
std::unique_ptr<CheckResult> performMultiObjectiveModelChecking(Environment const& env, SparseModelType const& model,
                                                                storm::logic::MultiObjectiveFormula const& formula, bool produceScheduler = false);

}
}  // namespace modelchecker
}  // namespace storm
