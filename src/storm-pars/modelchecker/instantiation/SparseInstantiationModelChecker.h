#pragma once

#include "storm-pars/utility/parametric.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/modelchecker/results/CheckResult.h"

namespace storm {

class Environment;

namespace modelchecker {

/*!
 * Class to efficiently check a formula on a parametric model with different parameter instantiations
 */
template<typename SparseModelType, typename ConstantType>
class SparseInstantiationModelChecker {
   public:
    SparseInstantiationModelChecker(SparseModelType const& parametricModel);
    virtual ~SparseInstantiationModelChecker() = default;

    void specifyFormula(CheckTask<storm::logic::Formula, typename SparseModelType::ValueType> const& checkTask);

    virtual std::unique_ptr<CheckResult> check(Environment const& env,
                                               storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) = 0;

    /**
     * Checks if the given valuation is valid for the current model.  Currently
     * used before checking whether the center of the model is SAT / UNSAT in
     * PLA. So we'll use the model instantiator to instantiate the model anyway,
     * but we currently don't re-use the result.
     *
     * @param valuation The valuation to check.
     * @return true if the valuation is well-defined, false otherwise.
     */
    virtual bool isWellDefined(storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) = 0;

    // If set, it is assumed that all considered model instantiations have the same underlying graph structure.
    // This bypasses the graph analysis for the different instantiations.
    void setInstantiationsAreGraphPreserving(bool value);
    bool getInstantiationsAreGraphPreserving() const;

    SparseModelType const& getOriginalModel() const;

   protected:
    SparseModelType const& parametricModel;
    std::unique_ptr<CheckTask<storm::logic::Formula, ConstantType>> currentCheckTask;

   private:
    // store the current formula. Note that currentCheckTask only stores a reference to the formula.
    std::shared_ptr<storm::logic::Formula const> currentFormula;

    bool instantiationsAreGraphPreserving;
};
}  // namespace modelchecker
}  // namespace storm
