#pragma once

#include "storm-counterexamples/counterexamples/CounterexampleOptions.h"
#include "storm-counterexamples/counterexamples/MILPMinimalLabelSetGenerator.h"
#include "storm-counterexamples/counterexamples/PathCounterexample.h"
#include "storm-counterexamples/counterexamples/SMTMinimalLabelSetGenerator.h"

namespace storm {
namespace api {

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMilp(
    storm::Environment const& env, storm::languages::SymbolicModelDescription const& symbolicModel, std::shared_ptr<storm::models::sparse::Mdp<double>> mdp,
    std::shared_ptr<storm::logic::Formula const> const& formula, bool includeSchedulerCuts = storm::counterexamples::CounterexampleOptions().useSchedulerCuts);

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMaxSmt(
    storm::Environment const& env, storm::languages::SymbolicModelDescription const& symbolicModel, std::shared_ptr<storm::models::sparse::Model<double>> model,
    std::shared_ptr<storm::logic::Formula const> const& formula,
    storm::counterexamples::SMTMinimalLabelSetGenerator<double>::Options const& options =
        storm::counterexamples::SMTMinimalLabelSetGenerator<double>::Options(true));

std::shared_ptr<storm::counterexamples::Counterexample> computeKShortestPathCounterexample(
    std::shared_ptr<storm::models::sparse::Model<double>> model, std::shared_ptr<storm::logic::Formula const> const& formula,
    size_t maxK = storm::counterexamples::CounterexampleOptions().shortestPathMaxK);

}  // namespace api
}  // namespace storm
