#pragma once

#include "storm/adapters/IntervalForward.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/solver/stateelimination/StatePriorityQueue.h"
#include "storm/storage/FlexibleSparseMatrix.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace modelchecker {

// forward declaration of friend class
namespace region {
template<typename ParametricModelType, typename ConstantType>
class SparseDtmcRegionModelChecker;
}

using namespace storm::solver::stateelimination;

template<typename SparseDtmcModelType>
class SparseDtmcEliminationModelChecker : public SparsePropositionalModelChecker<SparseDtmcModelType> {
    template<typename ParametricModelType, typename ConstantType>
    friend class storm::modelchecker::region::SparseDtmcRegionModelChecker;

   public:
    typedef typename SparseDtmcModelType::ValueType ValueType;
    typedef typename SparseDtmcModelType::RewardModelType RewardModelType;
    typedef typename storm::storage::FlexibleSparseMatrix<ValueType>::row_type FlexibleRowType;
    typedef typename FlexibleRowType::iterator FlexibleRowIterator;
    using SolutionType = storm::IntervalBaseType<ValueType>;

    /*!
     * Creates an elimination-based model checker for the given model.
     *
     * @param model The model to analyze.
     */
    explicit SparseDtmcEliminationModelChecker(storm::models::sparse::Dtmc<ValueType> const& model);

    // The implemented methods of the AbstractModelChecker interface.
    virtual bool canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env,
                                                                          CheckTask<storm::logic::BoundedUntilFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env,
                                                                    CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageRewards(
        Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env,
                                                                         CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageProbabilities(Environment const& env,
                                                                            CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) override;

    // Static helper methods
    static std::unique_ptr<CheckResult> computeUntilProbabilities(storm::storage::SparseMatrix<ValueType> const& probabilityMatrix,
                                                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                  storm::storage::BitVector const& initialStates, storm::storage::BitVector const& phiStates,
                                                                  storm::storage::BitVector const& psiStates, bool computeForInitialStatesOnly);

    static std::unique_ptr<CheckResult> computeReachabilityRewards(storm::storage::SparseMatrix<ValueType> const& probabilityMatrix,
                                                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                   storm::storage::BitVector const& initialStates,
                                                                   storm::storage::BitVector const& targetStates, std::vector<ValueType>& stateRewardValues,
                                                                   bool computeForInitialStatesOnly);

   private:
    static std::vector<SolutionType> computeLongRunValues(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                          storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                          storm::storage::BitVector const& initialStates, storm::storage::BitVector const& maybeStates,
                                                          bool computeResultsForInitialStatesOnly, std::vector<ValueType>& stateValues);

    static std::unique_ptr<CheckResult> computeReachabilityRewards(
        storm::storage::SparseMatrix<ValueType> const& probabilityMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
        storm::storage::BitVector const& initialStates, storm::storage::BitVector const& targetStates,
        std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
            totalStateRewardVectorGetter,
        bool computeForInitialStatesOnly);

    static std::vector<ValueType> computeReachabilityValues(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType>& values,
                                                            storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                            storm::storage::BitVector const& initialStates, bool computeResultsForInitialStatesOnly,
                                                            std::vector<ValueType> const& oneStepProbabilitiesToTarget);

    static void performPrioritizedStateElimination(std::shared_ptr<StatePriorityQueue>& priorityQueue,
                                                   storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                   storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, std::vector<ValueType>& values,
                                                   storm::storage::BitVector const& initialStates, bool computeResultsForInitialStatesOnly);

    static void performOrdinaryStateElimination(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                storm::storage::BitVector const& subsystem, storm::storage::BitVector const& initialStates,
                                                bool computeResultsForInitialStatesOnly, std::vector<ValueType>& values,
                                                boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities);

    static uint_fast64_t performHybridStateElimination(storm::storage::SparseMatrix<ValueType> const& forwardTransitions,
                                                       storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                       storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                       storm::storage::BitVector const& subsystem, storm::storage::BitVector const& initialStates,
                                                       bool computeResultsForInitialStatesOnly, std::vector<ValueType>& values,
                                                       boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities);

    static uint_fast64_t treatScc(storm::storage::FlexibleSparseMatrix<ValueType>& matrix, std::vector<ValueType>& values,
                                  storm::storage::BitVector const& entryStates, storm::storage::BitVector const& scc,
                                  storm::storage::BitVector const& initialStates, storm::storage::SparseMatrix<ValueType> const& forwardTransitions,
                                  storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, bool eliminateEntryStates, uint_fast64_t level,
                                  uint_fast64_t maximalSccSize, std::vector<storm::storage::sparse::state_type>& entryStateQueue,
                                  bool computeResultsForInitialStatesOnly,
                                  boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities = boost::none);

    static bool checkConsistent(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions);
};

}  // namespace modelchecker
}  // namespace storm
