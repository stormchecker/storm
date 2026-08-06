#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace storm {
namespace storage {
template<typename ValueType>
class SparseMatrix;
}

namespace modelchecker {
namespace helper {

template<typename ValueType>
class BaierUpperRewardBoundsComputer {
   public:
    /*!
     * Creates an object that can compute upper bounds on the *maximal* expected rewards for the provided MDP.
     * This also works with mixtures of positive and negative rewards.
     * The given matrix must not contain end components, i.e., under all strategies a target is reached almost surely.
     * @see http://doi.org/10.1007/978-3-319-63387-9_8
     * @param transitionMatrix The matrix defining the transitions of the system without the transitions
     * that lead directly to the goal state.
     * @param oneStepTargetProbabilities For each choice the probability to go to a goal state in one step.
     * @param stateToScc if given, the function has to assign to each state the index of its SCC. Useful if the SCC decomposition is already known from context.
     */
    BaierUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& oneStepTargetProbabilities,
                                   std::function<uint64_t(uint64_t)> const& stateToScc = {});

    /*!
     * Creates an object that can compute
     *    * upper bounds on the *maximal* expected rewards
     * for the provided MDP. This also works with mixtures of positive and negative rewards.
     * The given matrix must not contain end components, i.e., under all strategies a target is reached almost surely.
     * @see http://doi.org/10.1007/978-3-319-63387-9_8
     * @param transitionMatrix The matrix defining the transitions of the system without the transitions
     * that lead directly to the goal state.
     * @param backwardTransitions backward transitions
     * @param oneStepTargetProbabilities For each choice the probability to go to a goal state in one step.
     * @param stateToScc if given, the function has to assign to each state the index of its SCC. Useful if the SCC decomposition is already known from context.
     */
    BaierUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions, std::vector<ValueType> const& oneStepTargetProbabilities,
                                   std::function<uint64_t(uint64_t)> const& stateToScc = {});

    struct Bounds {
        ValueType lower;
        ValueType upper;
    };

    /*!
     * Computes a lower and an upper bound on the expected total rewards.
     * @param rewards The rewards of each choice. Mixtures of positive and negative rewards are allowed.
     */
    Bounds computeTotalRewardBounds(std::vector<ValueType> const& rewards);

    /*!
     * Computes for each state an upper bound for the maximal expected times each state is visited.
     * The given matrix must not contain end components, i.e., under all strategies a target is reached almost surely.
     * @param transitionMatrix The matrix defining the transitions of the system without the transitions
     * that lead directly to the goal state.
     * @param backwardTransitions backward transitions
     * @param oneStepTargetProbabilities For each choice the probability to go to a goal state in one step.
     * @param isMecCollapsingAllowedForChoice if a function is given, we allow to collapse MECs that only consist of choices where the given function returns
     * true. The interpretation of the returned visiting time bounds for states of such MECs is that visits of states that happen directly after a (collapsed)
     * mec choice do not count.
     */
    static std::vector<ValueType> computeUpperBoundOnExpectedVisitingTimes(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                           storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                           std::vector<ValueType> const& oneStepTargetProbabilities);

    /*!
     * Computes for each state an upper bound for the maximal expected times each state is visited.
     * The given matrix must not contain end components, i.e., under all strategies a target is reached almost surely.
     * @param transitionMatrix The matrix defining the transitions of the system without the transitions
     * that lead directly to the goal state.
     * @param oneStepTargetProbabilities For each choice the probability to go to a goal state in one step.
     */
    static std::vector<ValueType> computeUpperBoundOnExpectedVisitingTimes(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                           std::vector<ValueType> const& oneStepTargetProbabilities);

    /*!
     * Computes for each state an upper bound for the maximal expected times each state is visited.
     * The given matrix must not contain end components, i.e., under all strategies a target is reached almost surely.     *
     * @param transitionMatrix The matrix defining the transitions of the system without the transitions
     * that lead directly to the goal state.
     * @param oneStepTargetProbabilities For each choice the probability to go to a goal state in one step.
     * @param stateToScc Returns the SCC index for each state
     */
    static std::vector<ValueType> computeUpperBoundOnExpectedVisitingTimes(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                           storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                           std::vector<ValueType> const& oneStepTargetProbabilities,
                                                                           std::function<uint64_t(uint64_t)> const& stateToScc);

   private:
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    storm::storage::SparseMatrix<ValueType> const* backwardTransitions;
    std::function<uint64_t(uint64_t)> stateToScc;
    std::vector<ValueType> const& oneStepTargetProbabilities;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
