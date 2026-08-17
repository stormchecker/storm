#include "storm-pars/modelchecker/instantiation/SparseCtmcInstantiationModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseCtmcInstantiationModelChecker<SparseModelType, ConstantType>::SparseCtmcInstantiationModelChecker(SparseModelType const& parametricModel)
    : SparseInstantiationModelChecker<SparseModelType, ConstantType>(parametricModel), modelInstantiator(parametricModel) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseCtmcInstantiationModelChecker<SparseModelType, ConstantType>::check(
    Environment const& env, storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
    STORM_LOG_THROW(this->currentCheckTask, storm::exceptions::InvalidStateException, "Checking has been invoked but no property has been specified before.");
    auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
    storm::modelchecker::SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<ConstantType>> modelChecker(instantiatedModel);

    return modelChecker.check(env, *this->currentCheckTask);
}

template<typename SparseModelType, typename ConstantType>
bool SparseCtmcInstantiationModelChecker<SparseModelType, ConstantType>::isWellDefined(
    storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
    auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
    // Check that all rates are non-negative.
    for (auto const& entry : instantiatedModel.getTransitionMatrix()) {
        if (!storm::utility::isNonNegative(entry.getValue())) {
            return false;
        }
    }
    return true;
}

template class SparseCtmcInstantiationModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>, double>;
template class SparseCtmcInstantiationModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>, storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace storm
