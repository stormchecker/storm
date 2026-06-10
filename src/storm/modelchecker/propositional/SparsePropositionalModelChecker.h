#ifndef STORM_MODELCHECKER_SPARSEPROPOSITIONALMODELCHECKER_H_
#define STORM_MODELCHECKER_SPARSEPROPOSITIONALMODELCHECKER_H_

#include "storm/adapters/IntervalForward.h"
#include "storm/modelchecker/AbstractModelChecker.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType>
class SparsePropositionalModelChecker : public AbstractModelChecker<SparseModelType> {
   public:
    typedef typename SparseModelType::ValueType ValueType;
    typedef typename SparseModelType::RewardModelType RewardModelType;
    using SolutionType = IntervalBaseType<ValueType>;

    explicit SparsePropositionalModelChecker(SparseModelType const& model);

    // The implemented methods of the AbstractModelChecker interface.
    virtual bool canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const override;
    virtual std::unique_ptr<CheckResult> checkBooleanLiteralFormula(Environment const& env,
                                                                    CheckTask<storm::logic::BooleanLiteralFormula, SolutionType> const& checkTask) override;
    virtual std::unique_ptr<CheckResult> checkAtomicLabelFormula(Environment const& env,
                                                                 CheckTask<storm::logic::AtomicLabelFormula, SolutionType> const& checkTask) override;

   protected:
    /*!
     * Retrieves the model associated with this model checker instance.
     *
     * @return The model associated with this model checker instance.
     */
    SparseModelType const& getModel() const;

   private:
    // The model that is to be analyzed by the model checker.
    SparseModelType const& model;
};
}  // namespace modelchecker
}  // namespace storm

#endif /* STORM_MODELCHECKER_SPARSEPROPOSITIONALMODELCHECKER_H_ */
