#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"

#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"

#include "storm/adapters/JsonAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult() : truthValues(map_type()) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(map_type const& map) : truthValues(map) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(map_type&& map) : truthValues(map) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(storm::storage::sparse::state_type state, bool value) : truthValues(map_type()) {
    boost::get<map_type>(truthValues)[state] = value;
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(storm::storage::BitVector const& truthValues) : truthValues(truthValues) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(storm::storage::BitVector&& truthValues) : truthValues(std::move(truthValues)) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(boost::variant<vector_type, map_type> const& truthValues,
                                                                          std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : truthValues(truthValues), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQualitativeCheckResult<ValueType>::ExplicitQualitativeCheckResult(boost::variant<vector_type, map_type>&& truthValues,
                                                                          std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : truthValues(std::move(truthValues)), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQualitativeCheckResult<ValueType>::clone() const {
    return std::make_unique<ExplicitQualitativeCheckResult<ValueType>>(this->truthValues);
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::performLogicalOperation(ExplicitQualitativeCheckResult<ValueType>& first, QualitativeCheckResult const& second,
                                                                        bool logicalAnd) {
    STORM_LOG_THROW(second.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot perform logical 'and' on check results of incompatible type.");
    STORM_LOG_THROW(first.isResultForAllStates() == second.isResultForAllStates(), storm::exceptions::InvalidOperationException,
                    "Cannot perform logical 'and' on check results of incompatible type.");
    ExplicitQualitativeCheckResult<ValueType> const& secondCheckResult = static_cast<ExplicitQualitativeCheckResult<ValueType> const&>(second);
    if (first.isResultForAllStates()) {
        if (logicalAnd) {
            boost::get<vector_type>(first.truthValues) &= boost::get<vector_type>(secondCheckResult.truthValues);
        } else {
            boost::get<vector_type>(first.truthValues) |= boost::get<vector_type>(secondCheckResult.truthValues);
        }
    } else {
        std::function<bool(bool, bool)> function = logicalAnd ? std::function<bool(bool, bool)>([](bool a, bool b) { return a && b; })
                                                              : std::function<bool(bool, bool)>([](bool a, bool b) { return a || b; });

        map_type& map1 = boost::get<map_type>(first.truthValues);
        map_type const& map2 = boost::get<map_type>(secondCheckResult.truthValues);
        for (auto& element1 : map1) {
            auto const& keyValuePair = map2.find(element1.first);
            STORM_LOG_THROW(keyValuePair != map2.end(), storm::exceptions::InvalidOperationException,
                            "Cannot perform logical 'and' on check results of incompatible type.");
            element1.second = function(element1.second, keyValuePair->second);
        }

        // Double-check that there are no entries in map2 that the current result does not have.
        for (auto const& element2 : map2) {
            auto const& keyValuePair = map1.find(element2.first);
            STORM_LOG_THROW(keyValuePair != map1.end(), storm::exceptions::InvalidOperationException,
                            "Cannot perform logical 'and' on check results of incompatible type.");
        }
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
    if (this->isResultForAllStates()) {
        return !boost::get<vector_type>(truthValues).empty();
    } else {
        for (auto& element : boost::get<map_type>(truthValues)) {
            if (element.second) {
                return true;
            }
        }
        return false;
    }
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::forallTrue() const {
    if (this->isResultForAllStates()) {
        return boost::get<vector_type>(truthValues).full();
    } else {
        for (auto& element : boost::get<map_type>(truthValues)) {
            if (!element.second) {
                return false;
            }
        }
        return true;
    }
}

template<typename ValueType>
uint64_t ExplicitQualitativeCheckResult<ValueType>::count() const {
    if (this->isResultForAllStates()) {
        return boost::get<vector_type>(truthValues).getNumberOfSetBits();
    } else {
        uint64_t result = 0;
        for (auto& element : boost::get<map_type>(truthValues)) {
            if (element.second) {
                ++result;
            }
        }
        return result;
    }
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::operator[](storm::storage::sparse::state_type state) const {
    if (this->isResultForAllStates()) {
        return boost::get<vector_type>(truthValues).get(state);
    } else {
        map_type const& map = boost::get<map_type>(truthValues);
        auto const& keyValuePair = map.find(state);
        STORM_LOG_THROW(keyValuePair != map.end(), storm::exceptions::InvalidOperationException, "Unknown key '" << state << "'.");
        return keyValuePair->second;
    }
}

template<typename ValueType>
typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& ExplicitQualitativeCheckResult<ValueType>::getTruthValuesVector() const {
    return boost::get<vector_type>(truthValues);
}

template<typename ValueType>
typename ExplicitQualitativeCheckResult<ValueType>::map_type const& ExplicitQualitativeCheckResult<ValueType>::getTruthValuesMap() const {
    return boost::get<map_type>(truthValues);
}

template<typename ValueType>
void ExplicitQualitativeCheckResult<ValueType>::complement() {
    if (this->isResultForAllStates()) {
        boost::get<vector_type>(truthValues).complement();
    } else {
        for (auto& element : boost::get<map_type>(truthValues)) {
            element.second = !element.second;
        }
    }
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isResultForAllStates() const {
    return truthValues.which() == 0;
}

template<typename ValueType>
bool ExplicitQualitativeCheckResult<ValueType>::isExplicitQualitativeCheckResult() const {
    return true;
}

template<typename ValueType>
std::ostream& ExplicitQualitativeCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    if (this->isResultForAllStates()) {
        vector_type const& vector = boost::get<vector_type>(truthValues);
        bool allTrue = vector.full();
        bool allFalse = !allTrue && vector.empty();
        if (allTrue) {
            out << "{true}";
        } else if (allFalse) {
            out << "{false}";
        } else {
            out << "{true, false}";
        }
    } else {
        std::ios::fmtflags oldflags(std::cout.flags());
        out << std::boolalpha;

        map_type const& map = boost::get<map_type>(truthValues);
        if (map.size() == 1) {
            out << map.begin()->second;
        } else {
            bool allTrue = true;
            bool allFalse = true;
            for (auto const& entry : map) {
                if (entry.second) {
                    allFalse = false;
                } else {
                    allTrue = false;
                }
            }
            if (allTrue) {
                out << "{true}";
            } else if (allFalse) {
                out << "{false}";
            } else {
                out << "{true, false}";
            }
        }

        std::cout.flags(oldflags);
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

    if (this->isResultForAllStates()) {
        map_type newMap;
        for (auto element : filterTruthValues) {
            newMap.emplace(element, this->getTruthValuesVector().get(element));
        }
        this->truthValues = newMap;
    } else {
        map_type const& map = boost::get<map_type>(truthValues);

        map_type newMap;
        for (auto const& element : map) {
            if (filterTruthValues.get(element.first)) {
                newMap.insert(element);
            }
        }

        STORM_LOG_THROW(newMap.size() == filterTruthValues.getNumberOfSetBits(), storm::exceptions::InvalidOperationException,
                        "The check result fails to contain some results referred to by the filter.");

        this->truthValues = newMap;
    }
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
                     std::optional<storm::storage::sparse::StateValuations> const& stateValuations = std::nullopt,
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
storm::json<JsonRationalType> ExplicitQualitativeCheckResult<ValueType>::toJson(std::optional<storm::storage::sparse::StateValuations> const& stateValuations,
                                                                                std::optional<storm::models::sparse::StateLabeling> const& stateLabels) const {
    storm::json<JsonRationalType> result;
    if (this->isResultForAllStates()) {
        vector_type const& valuesAsVector = boost::get<vector_type>(truthValues);
        for (uint64_t state = 0; state < valuesAsVector.size(); ++state) {
            insertJsonEntry(result, state, valuesAsVector.get(state), stateValuations, stateLabels);
        }
    } else {
        map_type const& valuesAsMap = boost::get<map_type>(truthValues);
        for (auto const& stateValue : valuesAsMap) {
            insertJsonEntry(result, stateValue.first, stateValue.second, stateValuations, stateLabels);
        }
    }
    return result;
}

// Explicit template instantiations
template class ExplicitQualitativeCheckResult<double>;
template storm::json<double> ExplicitQualitativeCheckResult<double>::toJson<double>(std::optional<storm::storage::sparse::StateValuations> const&,
                                                                                    std::optional<storm::models::sparse::StateLabeling> const&) const;

template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<double>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::RationalNumber>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::RationalNumber>::toJson<double>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::RationalNumber>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::RationalFunction>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::RationalFunction>::toJson<double>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::RationalFunction>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

template class ExplicitQualitativeCheckResult<storm::Interval>;
template storm::json<double> ExplicitQualitativeCheckResult<storm::Interval>::toJson<double>(std::optional<storm::storage::sparse::StateValuations> const&,
                                                                                             std::optional<storm::models::sparse::StateLabeling> const&) const;
template storm::json<storm::RationalNumber> ExplicitQualitativeCheckResult<storm::Interval>::toJson<storm::RationalNumber>(
    std::optional<storm::storage::sparse::StateValuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const;

}  // namespace modelchecker
}  // namespace storm
