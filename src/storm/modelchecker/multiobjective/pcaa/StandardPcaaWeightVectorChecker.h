#pragma once

#include "storm/modelchecker/helper/infinitehorizon/SparseDeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseNondeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/multiobjective/Objective.h"
#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/solver/AbstractEquationSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

/*!
 * Helper Class that takes preprocessed Pcaa data and a weight vector and ...
 * - computes the optimal expected reward w.r.t. the weighted sum of the rewards of the individual objectives
 * - extracts the scheduler that induces this optimum
 * - computes for each objective the value induced by this scheduler
 */
template<class SparseModelType>
class StandardPcaaWeightVectorChecker : public PcaaWeightVectorChecker<SparseModelType> {
   public:
    typedef typename SparseModelType::ValueType ValueType;
    using DeterministicInfiniteHorizonHelperType =
        typename std::conditional<std::is_same<SparseModelType, storm::models::sparse::MarkovAutomaton<ValueType>>::value,
                                  storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>,
                                  storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType>>::type;

    /*
     * Creates a weight vextor checker.
     *
     * @param model The (preprocessed) model
     * @param objectives The (preprocessed) objectives
     * @param possibleECActions Overapproximation of the actions that are part of an EC
     * @param possibleBottomStates The states for which it is posible to not collect further reward with prob. 1
     *
     */
    StandardPcaaWeightVectorChecker(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult);

    /*!
     * - computes the optimal expected reward w.r.t. the weighted sum of the rewards of the individual objectives
     * - extracts the scheduler that induces this optimum
     * - computes for each objective the value induced by this scheduler
     */
    virtual void check(Environment const& env, std::vector<ValueType> weightVector) override;

    virtual std::vector<ValueType> getAchievablePoint() const override;
    ValueType getOptimalWeightedSum() const override;

    /*!
     * Retrieves a scheduler that induces the current values
     * Note that check(..) has to be called before retrieving the scheduler. Otherwise, an exception is thrown.
     * Also note that (currently) the scheduler only supports unbounded objectives.
     */
    virtual storm::storage::Scheduler<ValueType> computeScheduler() const override;

   protected:
    void initialize(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult);
    virtual void initializeModelTypeSpecificData(SparseModelType const& model) = 0;
    virtual storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> createNondetInfiniteHorizonHelper(
        storm::storage::SparseMatrix<ValueType> const& transitions) const = 0;
    virtual DeterministicInfiniteHorizonHelperType createDetInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitions) const = 0;

    void infiniteHorizonWeightedPhase(Environment const& env, std::vector<ValueType> const& weightedActionRewardVector,
                                      boost::optional<std::vector<ValueType>> const& weightedStateRewardVector, std::vector<ValueType> const& weightVector);

    /*!
     * Determines the scheduler that optimizes the weighted reward vector of the unbounded objectives
     *
     * @param weightedRewardVector the weighted rewards (only considering the unbounded objectives)
     */
    void unboundedWeightedPhase(Environment const& env, std::vector<ValueType> const& weightedRewardVector, std::vector<ValueType> const& weightVector);

    /*!
     * Computes the values of the objectives that do not have a stepBound w.r.t. the scheduler computed in the unboundedWeightedPhase
     *
     */
    void unboundedIndividualPhase(Environment const& env, std::vector<ValueType> const& weightVector);

    /*!
     * @return how the weighted precision (i.e. allowed approximation error) is distributed over the unbounded and bounded phase
     */
    virtual ValueType getWeightedPrecisionUnboundedPhase() const = 0;

    /*!
     * @return how the weighted precision (i.e. allowed approximation error) is distributed over the unbounded and bounded phase
     */
    virtual ValueType getWeightedPrecisionBoundedPhase() const = 0;

    /*!
     * For each time epoch (starting with the maximal stepBound occurring in the objectives), this method
     * - determines the objectives that are relevant in the current time epoch
     * - determines the maximizing scheduler for the weighted reward vector of these objectives
     * - computes the values of these objectives w.r.t. this scheduler
     *
     * @param weightVector the weight vector of the current check
     * @param weightedRewardVector the weighted rewards considering the unbounded objectives. Will be invalidated after calling this.
     */
    virtual void boundedPhase(Environment const& env, std::vector<ValueType> const& weightVector, std::vector<ValueType>& weightedRewardVector) = 0;

    void updateEcQuotient(std::vector<ValueType> const& weightedRewardVector);

    void setBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower, bool requiresUpper, uint64_t objIndex,
                           storm::storage::SparseMatrix<ValueType> const& transitions, storm::storage::BitVector const& rowsWithSumLessOne,
                           std::vector<ValueType> const& rewards) const;
    void setBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower, bool requiresUpper,
                           std::vector<ValueType> const& weightVector, storm::storage::BitVector const& objectiveFilter,
                           storm::storage::SparseMatrix<ValueType> const& transitions, storm::storage::BitVector const& rowsWithSumLessOne,
                           std::vector<ValueType> const& rewards) const;
    void computeAndSetBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower, bool requiresUpper,
                                     storm::storage::SparseMatrix<ValueType> const& transitions, storm::storage::BitVector const& rowsWithSumLessOne,
                                     std::vector<ValueType> const& rewards) const;

    /*!
     * Transforms the results of a min-max-solver that considers a reduced model (without end components) to a result for the original (unreduced) model
     */
    void transformEcqSolutionToOriginalModel(std::vector<ValueType> const& ecqSolution, std::vector<uint_fast64_t> const& ecqOptimalChoices,
                                             std::map<uint64_t, uint64_t> const& ecqStateToOptimalMecMap, std::vector<ValueType>& originalSolution,
                                             std::vector<uint_fast64_t>& originalOptimalChoices) const;

    // Data regarding the given model
    // The transition matrix of the considered model
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    // The initial state of the considered model
    uint64_t initialState;
    // Overapproximation of the set of choices that are part of an end component.
    storm::storage::BitVector ecChoicesHint;
    // The actions that have reward assigned for at least one objective without upper timeBound
    storm::storage::BitVector actionsWithoutRewardInUnboundedPhase;
    // The states for which there is a scheduler yielding reward 0 for each total reward objective
    storm::storage::BitVector totalReward0EStates;
    // stores the state action rewards for each objective.
    std::vector<std::vector<ValueType>> actionRewards;
    // stores the state rewards for each objective.
    // These are only relevant for LRA objectives for MAs (otherwise, they appear within the action rewards). For other objectives/models, the corresponding
    // vector will be empty.
    std::vector<std::vector<ValueType>> stateRewards;

    // stores the indices of the objectives for which we need to compute the long run average values
    storm::storage::BitVector lraObjectives;
    // stores the indices of the objectives for which there is no upper time bound
    storm::storage::BitVector objectivesWithNoUpperTimeBound;

    // Memory for the solution of the most recent call of check(..)
    // becomes true after the first call of check(..)
    bool checkHasBeenCalled;
    // The result for the weighted reward vector (for all states of the model)
    std::vector<ValueType> weightedResult;
    // The results for the individual objectives (w.r.t. all states of the model)
    std::vector<std::vector<ValueType>> objectiveResults;
    // Stores for each objective the offset between the computed result (w.r.t. the initial state) and a known achievable value (handling approximation errors)
    std::vector<ValueType> offsetsToAchievablePoint;
    // Stores an offset that is added to the weighted sum in order to obtain a sound upper bound (handling approximation errors)
    ValueType offsetToWeightedSum;
    // The scheduler choices that optimize the weighted rewards of undounded objectives.
    std::vector<uint64_t> optimalChoices;

    struct EcQuotient {
        storm::storage::SparseMatrix<ValueType> matrix;
        std::vector<uint_fast64_t> ecqToOriginalChoiceMapping;
        std::vector<uint_fast64_t> originalToEcqStateMapping;
        std::vector<storm::storage::FlatSetStateContainer> ecqToOriginalStateMapping;
        storm::storage::BitVector ecqStayInEcChoices;
        storm::storage::BitVector origReward0Choices;       // includes total and LRA rewards
        storm::storage::BitVector origTotalReward0Choices;  // considers just total rewards
        storm::storage::BitVector rowsWithSumLessOne;

        std::vector<ValueType> auxStateValues;
        std::vector<ValueType> auxChoiceValues;
    };
    boost::optional<EcQuotient> ecQuotient;

    // Merge results
    std::vector<uint64_t> goalStateMergerInputToReducedStateIndexMapping;
    std::vector<uint64_t> goalStateMergerReducedToInputChoiceMapping;

    struct LraMecDecomposition {
        storm::storage::MaximalEndComponentDecomposition<ValueType> mecs;
        std::vector<ValueType> auxMecValues;
    };
    boost::optional<LraMecDecomposition> lraMecDecomposition;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
