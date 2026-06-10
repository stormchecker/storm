#pragma once

#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"

namespace storm {

class Environment;

namespace modelchecker {
template<class SparseMdpModelType>
class SparseMdpPrctlModelChecker : public SparsePropositionalModelChecker<SparseMdpModelType> {
   public:
    typedef typename SparseMdpModelType::ValueType ValueType;
    typedef typename SparseMdpModelType::RewardModelType RewardModelType;
    using SolutionType = storm::IntervalBaseType<ValueType>;

    explicit SparseMdpPrctlModelChecker(SparseMdpModelType const& model);

    /*!
     * Returns false, if this task can certainly not be handled by this model checker (independent of the concrete model).
     * @param requiresSingleInitialState if not nullptr, this flag is set to true iff checking this formula requires a model with a single initial state
     */
    static bool canHandleStatic(CheckTask<storm::logic::Formula, SolutionType> const& checkTask, bool* requiresSingleInitialState = nullptr);

    // The implemented methods of the AbstractModelChecker interface.
    virtual bool canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env,
                                                                          CheckTask<storm::logic::BoundedUntilFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env,
                                                                  CheckTask<storm::logic::NextFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeGloballyProbabilities(Environment const& env,
                                                                      CheckTask<storm::logic::GloballyFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env,
                                                                         CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env,
                                                                  CheckTask<storm::logic::CumulativeRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeDiscountedCumulativeRewards(
        Environment const& env, CheckTask<storm::logic::DiscountedCumulativeRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeInstantaneousRewards(
        Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeTotalRewards(Environment const& env,
                                                             CheckTask<storm::logic::TotalRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeDiscountedTotalRewards(
        Environment const& env, CheckTask<storm::logic::DiscountedTotalRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env,
                                                                    CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env,
                                                                  CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageProbabilities(Environment const& env,
                                                                            CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageRewards(
        Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLTLProbabilities(Environment const& env,
                                                                 CheckTask<storm::logic::PathFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeHOAPathProbabilities(Environment const& env,
                                                                     CheckTask<storm::logic::HOAPathFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> checkMultiObjectiveFormula(Environment const& env,
                                                                    CheckTask<storm::logic::MultiObjectiveFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> checkLexObjectiveFormula(Environment const& env,
                                                                  CheckTask<storm::logic::MultiObjectiveFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> checkQuantileFormula(Environment const& env,
                                                              CheckTask<storm::logic::QuantileFormula, SolutionType> const& checkTask) override;
};
}  // namespace modelchecker
}  // namespace storm
