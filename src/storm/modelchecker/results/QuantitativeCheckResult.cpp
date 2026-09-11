#include "storm/modelchecker/results/QuantitativeCheckResult.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
std::unique_ptr<CheckResult> QuantitativeCheckResult<ValueType>::compareAgainstBound(storm::logic::ComparisonType, ValueType const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform comparison against bound on the check result.");
}

template<typename ValueType>
AggregatedValue<typename QuantitativeCheckResult<ValueType>::ExtendedValueType> QuantitativeCheckResult<ValueType>::aggregate(FilterType filter) const {
    switch (filter) {
        case FilterType::MIN:
            return {this->getMin(), std::nullopt, std::nullopt};
        case FilterType::MAX:
            return {this->getMax(), std::nullopt, std::nullopt};
        case FilterType::SUM:
            return {this->sum(), std::nullopt, std::nullopt};
        case FilterType::AVG:
            return {this->average(), std::nullopt, std::nullopt};
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "The filter " << toString(filter) << " does not aggregate values.");
    }
}

template<typename ValueType>
bool QuantitativeCheckResult<ValueType>::isQuantitative() const {
    return true;
}

template class QuantitativeCheckResult<double>;

template class QuantitativeCheckResult<RationalNumber>;
template class QuantitativeCheckResult<RationalFunction>;
}  // namespace modelchecker
}  // namespace storm
