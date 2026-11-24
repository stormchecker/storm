#pragma once

#include <map>
#include <memory>
#include <vector>

#include "storm-pars/scp/AffinePMdpDecomposition.h"
#include "storm-pars/scp/ScpLpEncoder.h"
#include "storm-pars/scp/ScpOptions.h"
#include "storm-pars/scp/ScpResult.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"
#include "storm/environment/Environment.h"

namespace storm::pars::scp {

/*!
 * Finds a parameter valuation satisfying an unbounded reachability threshold property (probability
 * or reward, `F` subformula) on an affine pMDP or pDTMC using Sequential Convex Programming
 * (Cubuktepe et al., "Convex Optimization for Parameter Synthesis in MDPs").
 *
 * SparseModelType is Mdp<RationalFunction> or Dtmc<RationalFunction>. ConstantType is the precision
 * of the model checks of candidate valuations and of ScpResult::value. With double, these are floating-point
 * model checks and are only as accurate as the numerical solvers; with storm::RationalNumber, they are exact.
 * Only the LP is always solved in double.
 *
 * For an Mdp the property is read robustly: AtLeast thresholds are checked against the minimal and
 * AtMost thresholds against the maximal value. An explicit Pmax/Pmin/Rmax/Rmin on the formula must
 * agree with this. A reward property uses the reward model named by the formula, else the model's
 * unique reward model.
 *
 * The region (the task's, or [0,1]^n if none is given) must be graph-preserving; this is not checked here.
 * Otherwise the model checks, which reuse the qualitative analysis of the region's center, may be wrong.
 *
 * A Feasible result is always verified by model-checking the returned valuation at ConstantType precision.
 * Any other status does not imply that no witness exists.
 */
template<typename SparseModelType, typename ConstantType>
class SparseScpInstantiationSearcher {
   public:
    explicit SparseScpInstantiationSearcher(SparseModelType const& model, ScpOptions options = ScpOptions());

    ScpResult<ConstantType> run(storm::Environment const& env, std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task);

   private:
    using ParametricType = typename SparseModelType::ValueType;
    using VariableType = typename storm::utility::parametric::VariableType<ParametricType>::type;
    using CoefficientType = typename storm::utility::parametric::CoefficientType<ParametricType>::type;
    using Valuation = storm::utility::parametric::Valuation<ParametricType>;

    /*!
     * The actual SCP iteration loop, shared between the Mdp and Dtmc branches of run() (which
     * differ only in which concrete instantiation-model-checker type they construct).
     */
    template<typename InstantiationChecker>
    ScpResult<ConstantType> runLoop(storm::Environment const& env, InstantiationChecker& instantiationChecker, ScpLpEncoder const& encoder,
                                    storm::logic::Bound const& bound, ThresholdDirection direction, uint64_t initialState,
                                    Valuation const& initialValuation) const;

    SparseModelType const& model;
    ScpOptions options;
};

}  // namespace storm::pars::scp
