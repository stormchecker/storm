#pragma once

#include <vector>

#include "storm/modelchecker/multiobjective/Objective.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/Scheduler.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace multiobjective {

/*!
 * Helper Class that takes a weight vector and ...
 * - computes the optimal value w.r.t. the weighted sum of the individual objectives
 * - extracts the scheduler that induces this optimum
 * - computes for each objective the value induced by this scheduler
 */
template<typename ModelType>
class PcaaWeightVectorChecker {
   public:
    typedef typename ModelType::ValueType ValueType;

    /*
     * Creates a weight vector checker.
     *
     * @param objectives The (preprocessed) objectives
     *
     */

    PcaaWeightVectorChecker(std::vector<Objective<ValueType>> const& objectives);

    virtual ~PcaaWeightVectorChecker() = default;

    /*!
     * Solves the Weighted Sum Optimization Problem for the given weight vector.
     * setWeightedPrecision(..) can be used to control the accuracy of the results.
     * After calling this, getAchievablePoint(), getOptimalWeightedSum(), and computeScheduler() can be invoked to retrieve the result of this call.
     * @note before calling this for the first time, a weighted precision needs to be set. Otherwise, an exception is thrown.
     * @note Minimizing objectives (Pmin=? [...], Rmin=? [...]) are *implicitly* negated, i.e.,
     *       we optimize sum_i p_i * (isMinimizing[i] ? -weightVector[i] : weightVector[i]), where p_i is the value of objective i
     *       For instance, a uniform weight vector like weightVector = (1, 1, ..., 1) will give equal weight to all (minimizing and maximizing) objectives.
     * @param env
     * @param weightVector the weight vector of the current check. There must be at least one non-zero entry.
     */
    virtual void check(Environment const& env, std::vector<ValueType> weightVector) = 0;

    /*!
     * Retrieves the result of the individual objectives at the initial state of the given model.
     * @note check(..) has to be called before retrieving results. Otherwise, an exception is thrown.
     * @note there is no guarantee that the point is achievable if the underlying solution method (e.g. standard value iteration) is unsound.
     * @note minimizing objectives are only negated implicitly, i.e. this function yields the actual (non-negated) objective values.
     *       Specifically, we have (using v_i for the exact value of objective i induced by the found scheduler):
     *       (*) for maximizing objective i, getAchievablePoint()[i] <= v_i
     *       (*) for minimizing objective i, getAchievablePoint()[i] >= v_i
     *       Here, equality holds if the underlying solution method (and value type) is exact and the weighted precision is set to 0.
     */
    virtual std::vector<ValueType> getAchievablePoint() const = 0;

    /*!
     * Retrieves the optimal weighted sum of the objective values (or an upper bound thereof).
     * @note check(..) has to be called before retrieving results. Otherwise, an exception is thrown.
     * @note there is no guarantee that the upper bound is sound if the underlying solution method (e.g. standard value iteration) is unsound.
     * @note minimizing objectives are handled by implicitly negating them, i.e. we optimize
     *       sum_i p_i * (isMinimizing[i] ? -weightVector[i] : weightVector[i]), where p_i is the value of objective i
     */
    virtual ValueType getOptimalWeightedSum() const = 0;

    /*!
     * Retrieves a scheduler that induces the current values (if such a scheduler was generated).
     * Note that check(..) has to be called before retrieving the scheduler. Otherwise, an exception is thrown.
     */
    virtual storm::storage::Scheduler<ValueType> computeScheduler() const;

    /*!
     * Sets the precision of this weight vector checker. After calling check() (with env.solver() set to sound or exact mode) the Euclidean distance between
     * (*) the found achievable point (with values for minimizing objectives negated) and
     * (*) the hyperplane with the given weightVector as normal vector and the found optimal weighted sum as offset
     * is at most the given weightedPrecision.
     */
    void setWeightedPrecision(ValueType const& value);

    /*!
     * Returns the current precision of this weight vector checker as specified by setWeightedPrecision().
     */
    ValueType const& getWeightedPrecision() const;

    /*!
     * Returns whether achieving precise values (i.e. low weighted precisions) might be challenging due to the type of objectives.
     * Currently, only time-bounded Markov automaton objectives are considered challenging.
     * This information is used to guide approximation heuristics, e.g. the choice of the approximation tradeoff in Pareto curve exploration.
     */
    virtual bool smallPrecisionsAreChallenging() const;

   protected:
    /*!
     * Computes the weighted lower or upper bounds for the provided set of objectives.
     * @param lower if true, lower result bounds are computed. otherwise upper result bounds
     * @param weightVector the weight vector of the current check
     */
    boost::optional<ValueType> computeWeightedResultBound(bool lower, std::vector<ValueType> const& weightVector,
                                                          storm::storage::BitVector const& objectiveFilter) const;

    // The (preprocessed) objectives
    std::vector<Objective<ValueType>> objectives;

   private:
    // The precision of this weight vector checker.
    std::optional<ValueType> weightedPrecision;
};

template<typename ModelType>
std::unique_ptr<PcaaWeightVectorChecker<ModelType>> createWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<ModelType> const& preprocessorResult);

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
