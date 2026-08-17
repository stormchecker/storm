#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename ValueType>
ExplicitParetoCurveCheckResult<ValueType>::ExplicitParetoCurveCheckResult() {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitParetoCurveCheckResult<ValueType>::ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state,
                                                                          std::vector<point_type> const& points, polytope_type const& underApproximation,
                                                                          polytope_type const& overApproximation)
    : ParetoCurveCheckResult<ValueType>(points, underApproximation, overApproximation), state(state) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitParetoCurveCheckResult<ValueType>::ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state, std::vector<point_type>&& points,
                                                                          polytope_type&& underApproximation, polytope_type&& overApproximation)
    : ParetoCurveCheckResult<ValueType>(points, underApproximation, overApproximation), state(state) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitParetoCurveCheckResult<ValueType>::ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state, std::vector<point_type>&& points,
                                                                          std::vector<scheduler_type>&& schedulers, polytope_type&& underApproximation,
                                                                          polytope_type&& overApproximation)
    : ParetoCurveCheckResult<ValueType>(points, underApproximation, overApproximation), state(state), schedulers(schedulers) {}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitParetoCurveCheckResult<ValueType>::clone() const {
    return std::make_unique<ExplicitParetoCurveCheckResult<ValueType>>(this->state, this->points, this->underApproximation, this->overApproximation);
}

template<typename ValueType>
bool ExplicitParetoCurveCheckResult<ValueType>::isExplicitParetoCurveCheckResult() const {
    return true;
}

template<typename ValueType>
bool ExplicitParetoCurveCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
void ExplicitParetoCurveCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    ExplicitQualitativeCheckResult<ValueType> const& explicitFilter = filter.template asExplicitQualitativeCheckResult<ValueType>();
    typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& filterTruthValues = explicitFilter.getTruthValuesVector();

    STORM_LOG_THROW(filterTruthValues.getNumberOfSetBits() == 1 && filterTruthValues.get(state), storm::exceptions::InvalidOperationException,
                    "The check result fails to contain some results referred to by the filter.");
}

template<typename ValueType>
storm::storage::sparse::state_type const& ExplicitParetoCurveCheckResult<ValueType>::getState() const {
    return state;
}

template<typename ValueType>
bool ExplicitParetoCurveCheckResult<ValueType>::hasScheduler() const {
    return schedulers.size() > 0;
}

template<typename ValueType>
std::vector<typename ExplicitParetoCurveCheckResult<ValueType>::scheduler_type> const& ExplicitParetoCurveCheckResult<ValueType>::getSchedulers() const {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return schedulers;
}

template<typename ValueType>
std::vector<typename ExplicitParetoCurveCheckResult<ValueType>::scheduler_type>& ExplicitParetoCurveCheckResult<ValueType>::getSchedulers() {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return schedulers;
}

template class ExplicitParetoCurveCheckResult<double>;
template class ExplicitParetoCurveCheckResult<storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace storm
