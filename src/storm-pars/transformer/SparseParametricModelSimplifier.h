#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <string>

#include "storm/logic/Formulas.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace transformer {

/*!
 * This class performs different steps to simplify the given (parametric) model.
 * Checking the obtained simplified formula on the simplified model yields the same result as checking the original formula on the original model (wrt. to the
 * initial states of the two models) End Components of nondeterministic models are removed whenever this is valid for the corresponding formula. This allows us
 * to apply, e.g., value iteration that does not start from the 0,...,0 vector.
 */
template<typename SparseModelType>
class SparseParametricModelSimplifier {
   public:
    SparseParametricModelSimplifier(SparseModelType const& model);
    virtual ~SparseParametricModelSimplifier() = default;

    /*
     * Invokes the simplification of the model w.r.t. the given formula.
     * Returns true, iff simplification was successful
     */
    bool simplify(storm::logic::Formula const& formula);

    /*
     * Retrieves the simplified model.
     * Note that simplify(formula) needs to be called first and has to return true. Otherwise an exception is thrown
     */
    std::shared_ptr<SparseModelType> getSimplifiedModel() const;

    /*
     * Retrieves the simplified formula.
     * Note that simplify(formula) needs to be called first and has to return true. Otherwise an exception is thrown
     */
    std::shared_ptr<storm::logic::Formula const> getSimplifiedFormula() const;

    /**
     * Set whether to preserve parametric transions (i.e. for robust PLA, where it returns unfavourable MCs).
     *
     * @param preserveParametricTransitions
     */
    void setPreserveParametricTransitions(bool preserveParametricTransitions);

    /**
     * Whether this SparseParametricDtmcSimplifier preserves parametric transitions.
     */
    bool isPreserveParametricTransitionsSet() const;

   protected:
    // Perform the simplification for the corresponding formula type
    virtual bool simplifyForUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula);
    virtual bool simplifyForReachabilityProbabilities(storm::logic::ProbabilityOperatorFormula const& formula);
    virtual bool simplifyForBoundedUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula);
    virtual bool simplifyForReachabilityRewards(storm::logic::RewardOperatorFormula const& formula);
    virtual bool simplifyForCumulativeRewards(storm::logic::RewardOperatorFormula const& formula);

    /*!
     * Eliminates all states that satisfy
     * * there is only one enabled action (i.e., there is no nondeterministic choice at the state),
     * * all outgoing transitions are constant,
     * * there is no statelabel defined, and
     * * (if rewardModelName is given) the reward collected at the state is constant.
     *
     * The resulting model will only have the rewardModel with the provided name (or no reward model at all if no name was given).
     * Labelings of eliminated states will be lost
     */
    std::shared_ptr<SparseModelType> eliminateConstantDeterministicStates(SparseModelType const& model, storm::storage::BitVector const& consideredStates,
                                                                          boost::optional<std::string> const& rewardModelName = boost::none);

    SparseModelType const& originalModel;

    std::shared_ptr<SparseModelType> simplifiedModel;
    std::shared_ptr<storm::logic::Formula const> simplifiedFormula;
    bool preserveParametricTransitions = false;
};
}  // namespace transformer
}  // namespace storm
