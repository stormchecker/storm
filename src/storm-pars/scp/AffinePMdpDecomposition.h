#pragma once

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm::pars::scp {

/*!
 * The affine decomposition of a single scalar quantity, so that everything downstream only ever
 * touches doubles:
 *
 *   value(v) = constant + sum_i coefficient_i * v_i
 *
 * where the sum ranges over the (few) parameters that actually occur in the quantity.
 */
struct UnpackedValue {
    double constant;
    std::vector<std::pair<storm::RationalFunctionVariable, double>> parameterCoefficients;
};

/*!
 * The affine part of a single transition (row, successor): UnpackedValue plus which successor
 * state it leads to.
 *
 *   P(row, successor)(v) = constant + sum_i coefficient_i * v_i
 */
struct UnpackedTransition {
    uint64_t successor;
    double constant;
    std::vector<std::pair<storm::RationalFunctionVariable, double>> parameterCoefficients;
};

/*!
 * Decomposes the transition matrix of an affine pMDP (or pDTMC), and optionally one reward model's
 * state-action rewards, into constant parts and (parameter, coefficient) pairs.
 *
 * Every transition probability and reward must be affine in the parameters (total degree <= 1);
 * otherwise construction throws. This makes the product P(s,alpha,s') * p_{s'} the only nonlinearity
 * in the Bellman inequalities. The decomposition is purely structural and independent of the
 * property's qualitative state sets.
 */
class AffinePMdpDecomposition {
   public:
    /*!
     * @param model A pMDP or pDTMC with storm::RationalFunction transition values.
     * @param rewardModel If given, this reward model's state-action reward vector is also
     * decomposed (see getReward()); its parameters are folded into getParameters() alongside the
     * transition matrix's. Must outlive this object. Leave as nullptr for a probability property,
     * which has no reward model to speak of.
     * @throws storm::exceptions::NotSupportedException if some transition (or, if rewardModel is
     * given, some state-action reward) is not affine in the parameters.
     */
    explicit AffinePMdpDecomposition(storm::models::sparse::Model<storm::RationalFunction> const& model,
                                     storm::models::sparse::StandardRewardModel<storm::RationalFunction> const* rewardModel = nullptr);

    /*!
     * The unpacked transitions of the given matrix row (a state for a DTMC, or a single
     * state-action choice for an MDP).
     */
    std::vector<UnpackedTransition> const& getRow(uint64_t row) const;

    /// The number of rows (matrix rows, i.e. choices) that were decomposed.
    uint64_t getNumberOfRows() const;

    /*!
     * The affine decomposition of the reward accrued by taking the given matrix row, from the
     * reward model passed to the constructor. Only valid to call if a reward model was actually
     * given there.
     */
    UnpackedValue const& getReward(uint64_t row) const;

    /// All parameters occurring anywhere in the model's transition matrix, or (if a reward model
    /// was given to the constructor) that reward model's state-action reward vector.
    std::set<storm::RationalFunctionVariable> const& getParameters() const;

   private:
    /*!
     * Decomposes a single affine RationalFunction into its constant part and (parameter,
     * coefficient) pairs, and folds its occurring parameters into `parameters`. Shared between the
     * transition-matrix loop and the (optional) reward-vector loop in the constructor.
     *
     * @pre `value` is affine in the parameters.
     */
    UnpackedValue decomposeAffine(storm::RationalFunction const& value);

    std::vector<std::vector<UnpackedTransition>> rows;
    std::vector<UnpackedValue> rewardRows;
    std::set<storm::RationalFunctionVariable> parameters;
};

}  // namespace storm::pars::scp
