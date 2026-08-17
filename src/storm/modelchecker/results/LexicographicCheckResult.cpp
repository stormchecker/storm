#include "LexicographicCheckResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
LexicographicCheckResult<ValueType>::LexicographicCheckResult(std::vector<ValueType> const& values, storm::storage::sparse::state_type state)
    : values(values), state(state) {}

template<typename ValueType>
std::vector<ValueType> const& LexicographicCheckResult<ValueType>::getInitialStateValue() const {
    return values;
}

template<typename ValueType>
storm::storage::sparse::state_type const& LexicographicCheckResult<ValueType>::getState() const {
    return state;
}

template<typename ValueType>
bool LexicographicCheckResult<ValueType>::isLexicographicCheckResult() const {
    return true;
}

template<typename ValueType>
std::unique_ptr<CheckResult> LexicographicCheckResult<ValueType>::clone() const {
    return std::make_unique<LexicographicCheckResult<ValueType>>(values, state);
}

template<typename ValueType>
bool LexicographicCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
void LexicographicCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    ExplicitQualitativeCheckResult<ValueType> const& explicitFilter = filter.template asExplicitQualitativeCheckResult<ValueType>();
    typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& filterTruthValues = explicitFilter.getTruthValuesVector();

    STORM_LOG_THROW(filterTruthValues.getNumberOfSetBits() == 1 && filterTruthValues.get(state), storm::exceptions::InvalidOperationException,
                    "The check result fails to contain some results referred to by the filter.");
}

template<typename ValueType>
std::ostream& LexicographicCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    out << "   (";
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it != values.begin()) {
            out << ", ";
        }
        out << std::setw(storm::NumberTraits<ValueType>::IsExact ? 20 : 11) << *it;
    }
    out << " )";
    if (storm::NumberTraits<ValueType>::IsExact) {
        out << " approx. ";
        out << "   (";
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (it != values.begin()) {
                out << ", ";
            }
            out << std::setw(11) << storm::utility::convertNumber<double>(*it);
        }
        out << " )";
    }
    out << '\n';
    return out;
}

template class LexicographicCheckResult<double>;
template class LexicographicCheckResult<storm::RationalNumber>;

}  // namespace modelchecker
}  // namespace storm
