#include "storm/modelchecker/results/SymbolicParetoCurveCheckResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type, typename ValueType>
SymbolicParetoCurveCheckResult<Type, ValueType>::SymbolicParetoCurveCheckResult() {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicParetoCurveCheckResult<Type, ValueType>::SymbolicParetoCurveCheckResult(
    storm::dd::Bdd<Type> const& state, std::vector<typename ParetoCurveCheckResult<ValueType>::point_type> const& points,
    typename ParetoCurveCheckResult<ValueType>::polytope_type const& underApproximation,
    typename ParetoCurveCheckResult<ValueType>::polytope_type const& overApproximation)
    : ParetoCurveCheckResult<ValueType>(points, underApproximation, overApproximation), state(state) {
    STORM_LOG_THROW(this->state.getNonZeroCount() == 1, storm::exceptions::InvalidOperationException,
                    "ParetoCheckResults are only relevant for a single state.");
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicParetoCurveCheckResult<Type, ValueType>::SymbolicParetoCurveCheckResult(storm::dd::Bdd<Type> const& state,
                                                                                std::vector<typename ParetoCurveCheckResult<ValueType>::point_type>&& points,
                                                                                typename ParetoCurveCheckResult<ValueType>::polytope_type&& underApproximation,
                                                                                typename ParetoCurveCheckResult<ValueType>::polytope_type&& overApproximation)
    : ParetoCurveCheckResult<ValueType>(points, underApproximation, overApproximation), state(state) {
    STORM_LOG_THROW(this->state.getNonZeroCount() == 1, storm::exceptions::InvalidOperationException,
                    "ParetoCheckResults are only relevant for a single state.");
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<CheckResult> SymbolicParetoCurveCheckResult<Type, ValueType>::clone() const {
    return std::make_unique<SymbolicParetoCurveCheckResult<Type, ValueType>>(this->state, this->points, this->underApproximation, this->overApproximation);
}

template<storm::dd::DdType Type, typename ValueType>
bool SymbolicParetoCurveCheckResult<Type, ValueType>::isSymbolicParetoCurveCheckResult() const {
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
bool SymbolicParetoCurveCheckResult<Type, ValueType>::isSymbolic() const {
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
void SymbolicParetoCurveCheckResult<Type, ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isSymbolicQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter Symbolic check result with non-Symbolic filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    SymbolicQualitativeCheckResult<Type> const& symbolicFilter = filter.template asSymbolicQualitativeCheckResult<Type>();

    storm::dd::Bdd<Type> const& filterTruthValues = symbolicFilter.getTruthValuesVector();

    STORM_LOG_THROW(filterTruthValues.getNonZeroCount() == 1 && !(filterTruthValues && state).isZero(), storm::exceptions::InvalidOperationException,
                    "The check result fails to contain some results referred to by the filter.");
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& SymbolicParetoCurveCheckResult<Type, ValueType>::getState() const {
    return state;
}

template class SymbolicParetoCurveCheckResult<storm::dd::DdType::CUDD, double>;
template class SymbolicParetoCurveCheckResult<storm::dd::DdType::Sylvan, double>;
template class SymbolicParetoCurveCheckResult<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace storm
