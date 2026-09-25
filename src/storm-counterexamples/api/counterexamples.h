#pragma once

#include "storm-counterexamples/counterexamples/MILPMinimalLabelSetGenerator.h"
#include "storm-counterexamples/counterexamples/PathCounterexample.h"
#include "storm-counterexamples/counterexamples/SMTMinimalLabelSetGenerator.h"

namespace storm {
namespace api {

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMilp(storm::Environment const& env,
                                                                                           storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                           std::shared_ptr<storm::models::sparse::Mdp<double>> mdp,
                                                                                           std::shared_ptr<storm::logic::Formula const> const& formula);

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMilp(storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                           std::shared_ptr<storm::models::sparse::Mdp<double>> mdp,
                                                                                           std::shared_ptr<storm::logic::Formula const> const& formula);

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMaxSmt(storm::Environment const& env,
                                                                                             storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                             std::shared_ptr<storm::models::sparse::Model<double>> model,
                                                                                             std::shared_ptr<storm::logic::Formula const> const& formula);

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMaxSmt(storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                             std::shared_ptr<storm::models::sparse::Model<double>> model,
                                                                                             std::shared_ptr<storm::logic::Formula const> const& formula);

std::shared_ptr<storm::counterexamples::Counterexample> computeKShortestPathCounterexample(std::shared_ptr<storm::models::sparse::Model<double>> model,
                                                                                           std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                                           size_t maxK);

}  // namespace api
}  // namespace storm
