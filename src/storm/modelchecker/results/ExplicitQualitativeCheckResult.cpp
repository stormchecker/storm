#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"

#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"

#include <algorithm>

#include "storm/adapters/JsonAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(storm::storage::sparse::state_type state, bool value)
    : states(storm::storage::BitVector(state + 1)), truthValues(1, value) {
    states->set(state);
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(vector_type const& truthValues,
                                                                          std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : truthValues(truthValues), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(vector_type&& truthValues,
                                                                          std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : truthValues(std::move(truthValues)), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(storm::storage::BitVector states, vector_type&& truthValues,
                                                                          std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : states(std::move(states)), truthValues(std::move(truthValues)), scheduler(scheduler) {
    STORM_LOG_ASSERT(this->states->getNumberOfSetBits() == this->truthValues.size(), "Expected one truth value per selected state.");
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQualitativeCheckResult<ValueType>::clone() const {
    return std::make_unique<ExplicitQualitativeCheckResult<ValueType>>(*this);
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::performLogicalOperation(ExplicitQualitativeCheckResult<ValueType>& first, QualitativeCheckResult const& second,
                                                                        bool logicalAnd) {
    STORM_LOG_THROW(second.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot perform logical 'and' on check results of incompatible type.");
    ExplicitQualitativeCheckResult<ValueType> const& secondCheckResult = static_cast<ExplicitQualitativeCheckResult<ValueType> const&>(second);
    STORM_LOG_THROW(first.states == secondCheckResult.states && first.truthValues.size() == secondCheckResult.truthValues.size(),
                    storm::exceptions::InvalidOperationException, "Cannot perform logical 'and' on check results of incompatible type.");
    if (logicalAnd) {
        first.truthValues &= secondCheckResult.truthValues;
    } else {
        first.truthValues |= secondCheckResult.truthValues;
    }
}

template<typename ValueType>
QualitativeCheckResult& ExplicitQualitativeCheckResult<ValueType>::operator&=(QualitativeCheckResult const& other) {
    performLogicalOperation(*this, other, true);
    return *this;
}

template<typename ValueType>
QualitativeCheckResult& ExplicitQualitativeCheckResult<ValueType>::operator|=(QualitativeCheckResult const& other) {
    performLogicalOperation(*this, other, false);
    return *this;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::existsTrue() const {
    return !truthValues.empty();
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::forallTrue() const {
    return truthValues.full();
}

template<typename ValueType>
uint64_t ExplicitQualitativeCheckResult<ValueType>::count() const {
    return truthValues.getNumberOfSetBits();
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::hasValueForState(storm::storage::sparse::state_type state) const {
    if (states) {
        return state < states->size() && states->get(state);
    }
    return state < truthValues.size();
}

template<typename ValueType>
uint64_t ExplicitQualitativeCheckResult<ValueType>::getOffset(storm::storage::sparse::state_type state) const {
    STORM_LOG_THROW(this->hasValueForState(state), storm::exceptions::InvalidOperationException, "Unknown key '" << state << "'.");
    return states ? states->getNumberOfSetBitsBeforeIndex(state) : state;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::operator[](storm::storage::sparse::state_type state) const {
    return truthValues.get(this->getOffset(state));
}

template<typename ValueType>
storm::storage::BitVector const& ExplicitQualitativeCheckResult<ValueType>::getStates() const {
    STORM_LOG_THROW(!this->isResultForAllStates(), storm::exceptions::InvalidOperationException,
                    "Unable to retrieve the states of a result that is for all states.");
    return *states;
}

template<typename ValueType>
typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& ExplicitQualitativeCheckResult<ValueType>::getTruthValuesVector() const {
    return truthValues;
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::complement() {
    truthValues.complement();
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isResultForAllStates() const {
    return !states.has_value();
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isExplicitQualitativeCheckResult() const {
    return true;
}

template<typename ValueType>
std::ostream& ExplicitQualitativeCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    if (!this->isResultForAllStates() && truthValues.size() == 1) {
        std::ios::fmtflags oldflags(out.flags());
        out << std::boolalpha << truthValues.get(0);
        out.flags(oldflags);
    } else if (truthValues.full()) {
        out << "{true}";
    } else if (truthValues.empty()) {
        out << "{false}";
    } else {
        out << "{true, false}";
    }
    return out;
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    ExplicitQualitativeCheckResult<ValueType> const& explicitFilter = filter.template asExplicitQualitativeCheckResult<ValueType>();
    vector_type const& filterTruthValues = explicitFilter.getTruthValuesVector();

    // Line the filter up with the states this result has truth values for. The two need not span the same range
    // of states, e.g. if this result holds a truth value for a single state only.
    uint64_t const numStates = std::max(filterTruthValues.size(), states ? states->size() : truthValues.size());
    storm::storage::BitVector available = states ? *states : storm::storage::BitVector(truthValues.size(), true);
    available.resize(numStates);
    storm::storage::BitVector selected(filterTruthValues);
    selected.resize(numStates);
    STORM_LOG_THROW(selected.isSubsetOf(available), storm::exceptions::InvalidOperationException,
                    "The check result fails to contain some results referred to by the filter.");

    truthValues = truthValues % (selected % available);
    states = filterTruthValues;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::hasScheduler() const {
    return static_cast<bool>(scheduler);
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::setScheduler(std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler) {
    this->scheduler = std::move(scheduler);
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> const& ExplicitQualitativeCheckResult<ValueType>::getScheduler() const {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return *scheduler.value();
}

template<typename ValueType>
storm::storage::Scheduler<ValueType>& ExplicitQualitativeCheckResult<ValueType>::getScheduler() {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return *scheduler.value();
}

template<typename JsonRationalType>
void insertJsonEntry(storm::json<JsonRationalType>& json, uint64_t const& id, bool value,
                     std::optional<storm::storage::sparse::Valuations> const& stateValuations = std::nullopt,
                     std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt) {
    storm::json<JsonRationalType> entry;
    if (stateValuations) {
        entry["s"] = stateValuations->template toJson<JsonRationalType>(id);
    } else {
        entry["s"] = id;
    }
    entry["v"] = value;
    if (stateLabels) {
        auto labs = stateLabels->getLabelsOfState(id);
        entry["l"] = labs;
    }
    json.push_back(std::move(entry));
}

template<typename ValueType>
template<typename JsonRationalType>
storm::json<JsonRationalType> ExplicitQualitativeCheckResult<ValueType>::toJson(std::optional<storm::storage::sparse::Valuations> const& stateValuations,
                                                                                std::optional<storm::models::sparse::StateLabeling> const& stateLabels) const {
    storm::json<JsonRationalType> result;
    this->forEachState([&](storm::storage::sparse::state_type state, uint64_t offset) {
        insertJsonEntry(result, state, truthValues.get(offset), stateValuations, stateLabels);
    });
    return result;
}

// Explicit template instantiations
template class ExplicitQualitativeCheckResult<double>;
template storm::json<double> ExplicitQualitativeCheckResult<double>::toJson<double>(std::optional<storm::storage::sparse::Valuations> const&,
                                                                                    std::optional<storm::models::sparse::StateLabeling> const&) const;

template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<double>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::RationalNumber>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::RationalNumber>::toJson<double>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::RationalNumber>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::RationalFunction>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::RationalFunction>::toJson<double>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::RationalFunction>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::Interval>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::Interval>::toJson<double>(std::optional<storm::storage::sparse::Valuations> const&,
                                                                                             std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::Interval>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

}  // namespace modelchecker
}  // namespace storm
