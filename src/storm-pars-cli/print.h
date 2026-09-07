#pragma once
#include "storm-pars/utility/parametric.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/utility/Stopwatch.h"

namespace storm::pars {
template<typename ValueType>
void printInitialStatesResult(std::unique_ptr<storm::modelchecker::CheckResult> const &result, storm::utility::Stopwatch *watch = nullptr,
                              const storm::utility::parametric::Valuation<ValueType> *valuation = nullptr);

}