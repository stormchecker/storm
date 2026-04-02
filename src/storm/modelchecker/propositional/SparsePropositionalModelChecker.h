#ifndef STORM_MODELCHECKER_SPARSEPROPOSITIONALMODELCHECKER_H_
#define STORM_MODELCHECKER_SPARSEPROPOSITIONALMODELCHECKER_H_

#include "storm/modelchecker/AbstractModelChecker.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType>
class SparsePropositionalModelChecker : public AbstractModelChecker<SparseModelType> {
   private:
    // Due to a GCC bug we have to add this dummy template type here
    // https://stackoverflow.com/questions/49707184/explicit-specialization-in-non-namespace-scope-does-not-compile-in-gcc
    template<typename T, typename Dummy>
    struct GetSolutionType {
        using type = T;
    };

    template<typename Dummy>
    struct GetSolutionType<storm::Interval, Dummy> {
        using type = double;
    };

    template<typename Dummy>
    struct GetSolutionType<storm::RationalInterval, Dummy> {
        using type = storm::RationalNumber;
    };

   public:
    typedef typename SparseModelType::ValueType ValueType;
    typedef typename SparseModelType::RewardModelType RewardModelType;
    using SolutionType = typename GetSolutionType<ValueType, void>::type;

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
