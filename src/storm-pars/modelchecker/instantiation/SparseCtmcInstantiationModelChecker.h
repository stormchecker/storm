#pragma once

#include <boost/optional.hpp>
#include <memory>

#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"
#include "storm-pars/utility/ModelInstantiator.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm {
namespace modelchecker {

/*!
 * Class to efficiently check a formula on a parametric model with different parameter instantiations.
 */
template<typename SparseModelType, typename ConstantType>
class SparseCtmcInstantiationModelChecker : public SparseInstantiationModelChecker<SparseModelType, ConstantType> {
   public:
    SparseCtmcInstantiationModelChecker(SparseModelType const& parametricModel);

    virtual std::unique_ptr<CheckResult> check(Environment const& env,
                                               storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) override;

    virtual bool isWellDefined(storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) override;

    storm::utility::ModelInstantiator<SparseModelType, storm::models::sparse::Ctmc<ConstantType>> modelInstantiator;
};
}  // namespace modelchecker
}  // namespace storm
