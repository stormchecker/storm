#pragma once

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "storm-pars/scp/AffinePMdpDecomposition.h"
#include "storm-pars/scp/ScpOptions.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/environment/Environment.h"
#include "storm/expressions/Variable.h"
#include "storm/solver/LpSolver.h"
#include "storm/storage/BitVector.h"

namespace storm::pars::scp {

/*!
 * Which side of the threshold a witness is searched for, read robustly over schedulers:
 *
 *   AtLeast (threshold is Greater/GreaterEqual) <-> the minimal value (Pmin / Rmin)
 *   AtMost  (threshold is Less/LessEqual)       <-> the maximal value (Pmax / Rmax)
 *
 * A Dtmc has no scheduler choice, but the same pairing selects the LP direction.
 */
enum class ThresholdDirection { AtLeast, AtMost };

/*!
 * The kind of property the LP is built for. Probability states take values in [0,1] and may have a
 * known value 1; reward states are unbounded above, have no known-value-1 case, and may have
 * infinite value (see ScpLpEncoder's excludedRows parameter).
 */
enum class PropertyKind { Probability, Reward };

/*!
 * The LP built for one SCP iteration, with the handles needed to read a solution back.
 * stateVariables is indexed by state; entries of states with a known value are default-constructed.
 */
struct EncodedLp {
    std::unique_ptr<storm::solver::LpSolver<double>> solver;
    std::vector<storm::expressions::Variable> stateVariables;
    std::map<storm::RationalFunctionVariable, storm::expressions::Variable> parameterVariables;
};

/*!
 * Builds the linearized-Bellman LP of one SCP iteration (Cubuktepe et al., "Convex Optimization for
 * Parameter Synthesis in MDPs"). Each bilinear term parameter * successor-value is replaced by its
 * first-order expansion around the current linearization point, within a multiplicative trust region.
 *
 * Everything that is fixed across iterations is given at construction; encode() depends only on the
 * linearization point and the trust region radius, and builds a fresh LP on every call.
 *
 * The Bellman inequality direction follows the threshold direction:
 *
 *   AtMost:  "p_s >= rhs - tau_s" for every action, objective minimizes p_{s_I}; converges to the
 *            least supersolution, i.e. Pmax. The caller supplies the Pmax qualitative sets.
 *   AtLeast: "p_s <= rhs + tau_s" for every action, objective maximizes p_{s_I}; converges to the
 *            greatest subsolution, i.e. Pmin. The caller supplies the Pmin qualitative sets.
 *
 * Pairing a maximizing objective with lower-bound-only constraints would let p_{s_I} grow to its
 * trust region ceiling independently of the parameters.
 */
class ScpLpEncoder {
   public:
    /*!
     * @param decomposition The affine decomposition of the model (and, for a reward property, of
     * the relevant reward model) -- must outlive this encoder.
     * @param rowGroupIndices The model's transition matrix row-group boundaries (state -> rows).
     * @param zeroStates States with known value 0: Prob0 (probability property) or Rew0, i.e. the
     * target states themselves (reward property). Excluded from LP-variable creation; contribute
     * nothing when a successor.
     * @param oneStates States with known value 1: Prob1 (probability property only -- pass an
     * empty BitVector for a reward property, which has no such state). Excluded from LP-variable
     * creation; when a successor, contribute their exact affine transition value (never
     * linearized, since multiplying by the *known constant* 1 introduces no bilinear term).
     * @param infiniteStates States with known value infinity: RewInf (reward property only -- pass
     * an empty BitVector for a probability property). Excluded from LP-variable creation and from
     * having a Bellman constraint built for them; by construction of `excludedRows` below, never
     * actually referenced as a successor of any row this encoder builds a constraint for.
     * @param excludedRows Rows to skip entirely when building Bellman constraints, beyond the
     * per-state exclusion above -- needed because a reward property's "value known" classification
     * is graph-based per *state*, but an individual action (row) of an otherwise-included MDP
     * state can still lead to an infiniteStates successor and must be excluded on its own (an MDP
     * state can have both safe and unsafe actions; a DTMC state cannot, so this is always empty
     * for DTMCs). Pass an all-clear BitVector (sized to the total row count) for a probability
     * property, which has no such case.
     * @param initialState The state whose value is compared against the threshold. Must not be a
     * zero/one/infinite state -- callers should short-circuit before building an LP in that
     * degenerate case, since the answer is then already known without solving anything.
     * @param parameterBounds The (lower, upper) bound of each parameter (i.e. the outer search
     * region -- the trust region is intersected with this, never outside it).
     * @param threshold The threshold to compare the initial state's value against.
     * @param direction Whether a valuation with value >= or <= threshold is being searched for.
     * @param propertyKind Probability (state variables bounded above by 1; no reward term in the
     * Bellman RHS) or Reward (state variables unbounded above; the Bellman RHS additionally
     * includes the row's affine reward, from decomposition.getReward()).
     */
    ScpLpEncoder(AffinePMdpDecomposition const& decomposition, std::vector<uint64_t> rowGroupIndices, storm::storage::BitVector zeroStates,
                 storm::storage::BitVector oneStates, storm::storage::BitVector infiniteStates, storm::storage::BitVector excludedRows, uint64_t initialState,
                 std::map<storm::RationalFunctionVariable, std::pair<double, double>> parameterBounds, double threshold, ThresholdDirection direction,
                 PropertyKind propertyKind, ScpOptions options);

    /*!
     * Builds the LP for one SCP iteration, linearized around (valuation, stateValues) with the
     * given trust region radius. Does not call solver->optimize() -- that is the caller's job.
     *
     * @param env The environment to build the LP solver with (e.g. solver-type selection); the
     * same one the caller uses for the instantiation model checker, so a caller's customized LP
     * solver choice is actually respected rather than silently falling back to a default.
     * @param valuation The current parameter linearization point; must contain every parameter
     * occurring in the model.
     * @param stateValues The current per-state linearization point (index = state); only entries
     * for "maybe" states (not zeroStates/oneStates/infiniteStates) are read.
     * @param trustRegionRadius The multiplicative trust region radius (>= 1); see ScpOptions.
     */
    EncodedLp encode(storm::Environment const& env, std::map<storm::RationalFunctionVariable, double> const& valuation, std::vector<double> const& stateValues,
                     double trustRegionRadius) const;

   private:
    AffinePMdpDecomposition const& decomposition;
    std::vector<uint64_t> rowGroupIndices;
    storm::storage::BitVector zeroStates;
    storm::storage::BitVector oneStates;
    storm::storage::BitVector infiniteStates;
    storm::storage::BitVector excludedRows;
    uint64_t initialState;
    std::map<storm::RationalFunctionVariable, std::pair<double, double>> parameterBounds;
    double threshold;
    ThresholdDirection direction;
    PropertyKind propertyKind;
    ScpOptions options;
};

}  // namespace storm::pars::scp
