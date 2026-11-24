#pragma once

#include <memory>
#include <optional>

#include "storm-pars/modelchecker/region/RegionCheckEngine.h"
#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm-pars/utility/FeasibilityResult.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"
#include "storm/environment/Environment.h"
#include "storm/models/sparse/Model.h"

namespace storm::pars {

/*!
 * Feasibility decision: ExistsSat (witness found), AllViolated (no satisfying valuation), or Unknown.
 */
template<typename ConstantType>
struct FeasibilitySynthesisResult {
    storm::modelchecker::RegionResult status = storm::modelchecker::RegionResult::Unknown;
    /// Valuation and value, present only for ExistsSat.
    std::optional<FeasibilityResult<ConstantType>> witness;
};

}  // namespace storm::pars

namespace storm::api {

/*!
 * Checks a bounded probability or reward formula for all well-defined valuations in the region.
 * Requires a sparse DTMC or MDP with one initial state.
 * The region must cover all model parameters and satisfy the engine's requirements.
 * Throws for unsupported input.
 */
bool verify(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
            std::shared_ptr<storm::logic::Formula const> const& formula, storm::storage::ParameterRegion<storm::RationalFunction> const& region,
            storm::modelchecker::RegionCheckEngine engine = storm::modelchecker::RegionCheckEngine::ParameterLifting);

/*!
 * Checks a bounded probability or reward formula for all well-defined valuations in the region.
 * Requires a sparse DTMC or MDP with one initial state.
 * Uses [regionBound, 1-regionBound] for every parameter. regionBound must be in [0, 1/2].
 * The region must satisfy the engine's requirements.
 * Throws for unsupported input.
 */
bool verify(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
            std::shared_ptr<storm::logic::Formula const> const& formula, storm::RationalFunctionCoefficient const& regionBound,
            storm::modelchecker::RegionCheckEngine engine = storm::modelchecker::RegionCheckEngine::ParameterLifting);

/*!
 * Finds a well-defined valuation in the region satisfying a bounded probability or reward formula.
 * Requires a sparse DTMC or MDP with one initial state.
 * The region must cover all model parameters and satisfy the method's and engine's requirements.
 * regionEngine applies only to PLA.
 * On MDPs, Pmax>=0.8 [F "target"] asks for some scheduler to reach "target" with probability at least 0.8 (PLA only).
 * Pmin>=0.8 [F "target"] requires this of every scheduler.
 * Without min/max, bounds apply to every scheduler. SCP requires this interpretation.
 * Throws for unsupported input.
 */
storm::pars::FeasibilitySynthesisResult<double> checkFeasibility(
    storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
    std::shared_ptr<storm::logic::Formula const> const& formula, storm::storage::ParameterRegion<storm::RationalFunction> const& region,
    storm::pars::FeasibilityMethod method = storm::pars::FeasibilityMethod::PLA,
    storm::modelchecker::RegionCheckEngine regionEngine = storm::modelchecker::RegionCheckEngine::ParameterLifting);

/*!
 * Finds a well-defined valuation in the region satisfying a bounded probability or reward formula.
 * Requires a sparse DTMC or MDP with one initial state.
 * Uses [regionBound, 1-regionBound] for every parameter. regionBound must be in [0, 1/2].
 * The region must satisfy the method's and engine's requirements.
 * regionEngine applies only to PLA.
 * On MDPs, Pmax>=0.8 [F "target"] asks for some scheduler to reach "target" with probability at least 0.8 (PLA only).
 * Pmin>=0.8 [F "target"] requires this of every scheduler.
 * Without min/max, bounds apply to every scheduler. SCP requires this interpretation.
 * Throws for unsupported input.
 */
storm::pars::FeasibilitySynthesisResult<double> checkFeasibility(
    storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
    std::shared_ptr<storm::logic::Formula const> const& formula, storm::RationalFunctionCoefficient const& regionBound,
    storm::pars::FeasibilityMethod method = storm::pars::FeasibilityMethod::PLA,
    storm::modelchecker::RegionCheckEngine regionEngine = storm::modelchecker::RegionCheckEngine::ParameterLifting);

}  // namespace storm::api
