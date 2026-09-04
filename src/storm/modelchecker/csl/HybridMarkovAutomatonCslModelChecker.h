#pragma once

#include "storm/modelchecker/propositional/SymbolicPropositionalModelChecker.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/numbers/NumberTraits.h"
#include "storm/solver/LinearEquationSolver.h"

namespace storm {

namespace modelchecker {

template<typename ModelType>
class HybridMarkovAutomatonCslModelChecker : public SymbolicPropositionalModelChecker<ModelType> {
   public:
    typedef typename ModelType::ValueType ValueType;
    static const storm::dd::DdType DdType = ModelType::DdType;

    explicit HybridMarkovAutomatonCslModelChecker(ModelType const& model);

    // The implemented methods of the AbstractModelChecker interface.
    static bool canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask);
    virtual bool canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env,
                                                                          CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env,
                                                                    CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env,
                                                                  CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageProbabilities(Environment const& env,
                                                                            CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageRewards(
        Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) override;
};

}  // namespace modelchecker
}  // namespace storm
