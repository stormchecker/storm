#pragma once

#include "storm/modelchecker/propositional/SymbolicPropositionalModelChecker.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/numbers/NumberTraits.h"
#include "storm/solver/LinearEquationSolver.h"

namespace storm {
namespace modelchecker {

template<typename ModelType>
class HybridCtmcCslModelChecker : public SymbolicPropositionalModelChecker<ModelType> {
   public:
    typedef typename ModelType::ValueType ValueType;
    static const storm::dd::DdType DdType = ModelType::DdType;

    explicit HybridCtmcCslModelChecker(ModelType const& model);

    // Returns false, if this task can certainly not be handled by this model checker (independent of the concrete model).
    static bool canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask);

    // The implemented methods of the AbstractModelChecker interface.
    virtual bool canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env,
                                                                          CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env,
                                                                  CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeLongRunAverageProbabilities(Environment const& env,
                                                                            CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) override;

    virtual std::unique_ptr<CheckResult> computeLongRunAverageRewards(
        Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeInstantaneousRewards(Environment const& env,
                                                                     CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env,
                                                                  CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env,
                                                                    CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env,
                                                                  CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;
};

}  // namespace modelchecker
}  // namespace storm
