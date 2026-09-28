#pragma once

#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace modelchecker {

template<class SparseCtmcModelType>
class SparseCtmcCslModelChecker : public SparsePropositionalModelChecker<SparseCtmcModelType> {
   public:
    typedef typename SparseCtmcModelType::ValueType ValueType;
    typedef storm::utility::ExtendedValueType<ValueType> ExtendedValueType;
    typedef typename SparseCtmcModelType::RewardModelType RewardModelType;

    explicit SparseCtmcCslModelChecker(SparseCtmcModelType const& model);

    // Returns false, if this task can certainly not be handled by this model checker (independent of the concrete model).
    static bool canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask);

    // The implemented methods of the AbstractModelChecker interface.
    virtual bool canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env,
                                                                          CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env,
                                                                  CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeGloballyProbabilities(Environment const& env,
                                                                      CheckTask<storm::logic::GloballyFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLTLProbabilities(Environment const& env,
                                                                 CheckTask<storm::logic::PathFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeHOAPathProbabilities(Environment const& env,
                                                                     CheckTask<storm::logic::HOAPathFormula, ValueType> const& checkTask) override;

    virtual std::unique_ptr<CheckResult> computeLongRunAverageProbabilities(Environment const& env,
                                                                            CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env,
                                                                  CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;

    virtual std::unique_ptr<CheckResult> computeLongRunAverageRewards(
        Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env,
                                                                  CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeInstantaneousRewards(Environment const& env,
                                                                     CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env,
                                                                    CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeTotalRewards(Environment const& env,
                                                             CheckTask<storm::logic::TotalRewardFormula, ValueType> const& checkTask) override;

    /*!
     * Compute transient probabilities for all states.
     */
    std::vector<ValueType> computeAllTransientProbabilities(Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask);

    /*!
     * Computes the long run average (or: steady state) distribution over all states
     * Assumes a uniform distribution over initial states.
     */
    std::unique_ptr<CheckResult> computeSteadyStateDistribution(Environment const& env);

    /*!
     * Computes for each state the expected number of times we visit that state.
     * Assumes a uniform distribution over initial states.
     */
    std::unique_ptr<CheckResult> computeExpectedVisitingTimes(Environment const& env);
};

}  // namespace modelchecker
}  // namespace storm
