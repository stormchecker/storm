#pragma once

#include <memory>

#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"
#include "storm-pars/utility/ModelInstantiator.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm {
namespace modelchecker {

/*!
 * Class to efficiently check a formula on a parametric model with different parameter instantiations
 */
template<typename SparseModelType, typename ConstantType>
class SparseMdpInstantiationModelChecker : public SparseInstantiationModelChecker<SparseModelType, ConstantType> {
   public:
    SparseMdpInstantiationModelChecker(SparseModelType const& parametricModel, bool produceScheduler = true);

    virtual std::unique_ptr<CheckResult> check(Environment const& env,
                                               storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) override;

    virtual bool isWellDefined(storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) override;

   protected:
    // Optimizations for the different formula types
    std::unique_ptr<CheckResult> checkReachabilityProbabilityFormula(
        Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker,
        storm::models::sparse::Mdp<ConstantType> const& instantiatedModel);
    std::unique_ptr<CheckResult> checkReachabilityRewardFormula(
        Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker,
        storm::models::sparse::Mdp<ConstantType> const& instantiatedModel);
    std::unique_ptr<CheckResult> checkBoundedUntilFormula(
        Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker);

    storm::utility::ModelInstantiator<SparseModelType, storm::models::sparse::Mdp<ConstantType>> modelInstantiator;
    bool produceScheduler;
};
}  // namespace modelchecker
}  // namespace storm
