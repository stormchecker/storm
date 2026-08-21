#include "storm/adapters/RationalNumberAdapter.h"  // Must come first. TODO: fix

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

namespace detail {
/*!
 * Restricts the given values to the states selected by the filter. Note that the result is always a map, even
 * if the given values are a vector.
 */
template<typename ValueType>
boost::variant<std::vector<ValueType>, std::map<storm::storage::sparse::state_type, ValueType>> filterValues(
    boost::variant<std::vector<ValueType>, std::map<storm::storage::sparse::state_type, ValueType>> const& values,
    storm::storage::BitVector const& filterTruthValues) {
    typedef std::vector<ValueType> vector_type;
    typedef std::map<storm::storage::sparse::state_type, ValueType> map_type;

    map_type newMap;
    if (values.which() == 0) {
        vector_type const& valuesAsVector = boost::get<vector_type>(values);
        for (auto element : filterTruthValues) {
            STORM_LOG_THROW(element < valuesAsVector.size(), storm::exceptions::InvalidAccessException, "Invalid index in results.");
            newMap.emplace(element, valuesAsVector[element]);
        }
    } else {
        for (auto const& element : boost::get<map_type>(values)) {
            if (filterTruthValues.get(element.first)) {
                newMap.insert(element);
            }
        }
        STORM_LOG_THROW(newMap.size() == filterTruthValues.getNumberOfSetBits(), storm::exceptions::InvalidOperationException,
                        "The check result fails to contain some results referred to by the filter.");
    }
    return newMap;
}

/*!
 * Replaces every value by one minus that value.
 */
template<typename ValueType>
void complementValues(boost::variant<std::vector<ValueType>, std::map<storm::storage::sparse::state_type, ValueType>>& values) {
    typedef std::vector<ValueType> vector_type;
    typedef std::map<storm::storage::sparse::state_type, ValueType> map_type;

    if (values.which() == 0) {
        for (auto& element : boost::get<vector_type>(values)) {
            element = storm::utility::one<ValueType>() - element;
        }
    } else {
        for (auto& element : boost::get<map_type>(values)) {
            element.second = storm::utility::one<ValueType>() - element.second;
        }
    }
}
}  // namespace detail

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult() : values(map_type()) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(map_type const& values) : values(values) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(map_type&& values) : values(std::move(values)) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(storm::storage::sparse::state_type const& state, ValueType const& value)
    : values(map_type()) {
    boost::get<map_type>(values).emplace(state, value);
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(vector_type const& values) : values(values) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(vector_type&& values) : values(std::move(values)) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(boost::variant<vector_type, map_type> const& values,
                                                                            std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : values(values), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(boost::variant<vector_type, map_type>&& values,
                                                                            std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : values(std::move(values)), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(ExplicitQualitativeCheckResult<ValueType> const& other) {
    if (other.isResultForAllStates()) {
        storm::storage::BitVector const& bvValues = other.getTruthValuesVector();

        vector_type newVector;
        newVector.reserve(bvValues.size());
        for (std::size_t i = 0, n = bvValues.size(); i < n; i++) {
            newVector.push_back(bvValues.get(i) ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>());
        }

        values = newVector;
    } else {
        typename ExplicitQualitativeCheckResult<ValueType>::map_type const& bitMap = other.getTruthValuesMap();

        map_type newMap;
        for (auto const& e : bitMap) {
            newMap[e.first] = e.second ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>();
        }

        values = newMap;
    }
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<ValueType>::clone() const {
    auto result = std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>(this->values, this->scheduler);
    result->lowerBounds = this->lowerBounds;
    result->upperBounds = this->upperBounds;
    return result;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getValueVector() const {
    return boost::get<vector_type>(values);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type& ExplicitQuantitativeCheckResult<ValueType>::getValueVector() {
    return boost::get<vector_type>(values);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::map_type const& ExplicitQuantitativeCheckResult<ValueType>::getValueMap() const {
    return boost::get<map_type>(values);
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasLowerBounds() const {
    return lowerBounds.has_value();
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasUpperBounds() const {
    return upperBounds.has_value();
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getLowerBoundVector() const {
    STORM_LOG_THROW(this->hasLowerBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown lower bounds.");
    return boost::get<vector_type>(*lowerBounds);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getUpperBoundVector() const {
    STORM_LOG_THROW(this->hasUpperBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown upper bounds.");
    return boost::get<vector_type>(*upperBounds);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::map_type const& ExplicitQuantitativeCheckResult<ValueType>::getLowerBoundMap() const {
    STORM_LOG_THROW(this->hasLowerBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown lower bounds.");
    return boost::get<map_type>(*lowerBounds);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::map_type const& ExplicitQuantitativeCheckResult<ValueType>::getUpperBoundMap() const {
    STORM_LOG_THROW(this->hasUpperBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown upper bounds.");
    return boost::get<map_type>(*upperBounds);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::assertBoundsShape([[maybe_unused]] boost::variant<vector_type, map_type> const& bounds) const {
    STORM_LOG_ASSERT(bounds.which() == values.which(), "Bounds must have the same shape as the values.");
    if (this->isResultForAllStates()) {
        STORM_LOG_ASSERT(boost::get<vector_type>(bounds).size() == boost::get<vector_type>(values).size(), "Bounds must have the same size as the values.");
    } else {
        STORM_LOG_ASSERT(boost::get<map_type>(bounds).size() == boost::get<map_type>(values).size(), "Bounds must have the same size as the values.");
    }
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setLowerBounds(boost::variant<vector_type, map_type> lowerBounds) {
    this->assertBoundsShape(lowerBounds);
    this->lowerBounds = std::move(lowerBounds);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setUpperBounds(boost::variant<vector_type, map_type> upperBounds) {
    this->assertBoundsShape(upperBounds);
    this->upperBounds = std::move(upperBounds);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setBounds(boost::variant<vector_type, map_type> lowerBounds,
                                                           boost::variant<vector_type, map_type> upperBounds) {
    this->setLowerBounds(std::move(lowerBounds));
    this->setUpperBounds(std::move(upperBounds));
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::clearBounds() {
    lowerBounds = std::nullopt;
    upperBounds = std::nullopt;
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    STORM_LOG_THROW(filter.hasValueType<ValueType>(), storm::exceptions::InvalidOperationException, "Filter has unexpected value type.");
    ExplicitQualitativeCheckResult<ValueType> const& explicitFilter = filter.template asExplicitQualitativeCheckResult<ValueType>();
    typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& filterTruthValues = explicitFilter.getTruthValuesVector();

    if (this->hasLowerBounds()) {
        this->lowerBounds = detail::filterValues<ValueType>(*this->lowerBounds, filterTruthValues);
    }
    if (this->hasUpperBounds()) {
        this->upperBounds = detail::filterValues<ValueType>(*this->upperBounds, filterTruthValues);
    }
    this->values = detail::filterValues<ValueType>(this->values, filterTruthValues);
}

template<typename ValueType>
ValueType ExplicitQuantitativeCheckResult<ValueType>::getMin() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Minimum of empty set is not defined.");

    if (this->isResultForAllStates()) {
        return storm::utility::minimum(boost::get<vector_type>(values));
    } else {
        return storm::utility::minimum(boost::get<map_type>(values));
    }
}

template<typename ValueType>
ValueType ExplicitQuantitativeCheckResult<ValueType>::getMax() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Minimum of empty set is not defined.");

    if (this->isResultForAllStates()) {
        return storm::utility::maximum(boost::get<vector_type>(values));
    } else {
        return storm::utility::maximum(boost::get<map_type>(values));
    }
}

template<typename ValueType>
std::pair<ValueType, ValueType> ExplicitQuantitativeCheckResult<ValueType>::getMinMax() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Minimum/maximum of empty set is not defined.");

    if (this->isResultForAllStates()) {
        return storm::utility::minmax(boost::get<vector_type>(values));
    } else {
        return storm::utility::minmax(boost::get<map_type>(values));
    }
}

template<typename ValueType>
ValueType ExplicitQuantitativeCheckResult<ValueType>::sum() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Sum of empty set is not defined.");

    ValueType sum = storm::utility::zero<ValueType>();
    if (this->isResultForAllStates()) {
        for (auto& element : boost::get<vector_type>(values)) {
            STORM_LOG_THROW(element != storm::utility::infinity<ValueType>(), storm::exceptions::InvalidOperationException,
                            "Cannot compute the sum of values containing infinity.");
            sum += element;
        }
    } else {
        for (auto& element : boost::get<map_type>(values)) {
            STORM_LOG_THROW(element.second != storm::utility::infinity<ValueType>(), storm::exceptions::InvalidOperationException,
                            "Cannot compute the sum of values containing infinity.");
            sum += element.second;
        }
    }
    return sum;
}

template<typename ValueType>
ValueType ExplicitQuantitativeCheckResult<ValueType>::average() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Average of empty set is not defined.");

    ValueType sum = storm::utility::zero<ValueType>();
    if (this->isResultForAllStates()) {
        for (auto& element : boost::get<vector_type>(values)) {
            STORM_LOG_THROW(element != storm::utility::infinity<ValueType>(), storm::exceptions::InvalidOperationException,
                            "Cannot compute the average of values containing infinity.");
            sum += element;
        }
        return sum / boost::get<vector_type>(values).size();
    } else {
        for (auto& element : boost::get<map_type>(values)) {
            STORM_LOG_THROW(element.second != storm::utility::infinity<ValueType>(), storm::exceptions::InvalidOperationException,
                            "Cannot compute the average of values containing infinity.");
            sum += element.second;
        }
        return sum / boost::get<map_type>(values).size();
    }
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasScheduler() const {
    return static_cast<bool>(scheduler);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setScheduler(std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler) {
    this->scheduler = std::move(scheduler);
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> const& ExplicitQuantitativeCheckResult<ValueType>::getScheduler() const {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return *scheduler.value();
}

template<typename ValueType>
storm::storage::Scheduler<ValueType>& ExplicitQuantitativeCheckResult<ValueType>::getScheduler() {
    STORM_LOG_THROW(this->hasScheduler(), storm::exceptions::InvalidOperationException, "Unable to retrieve non-existing scheduler.");
    return *scheduler.value();
}

template<typename ValueType>
void print(std::ostream& out, ValueType const& value) {
    if (value == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << value;
        if (std::is_same<ValueType, storm::RationalNumber>::value) {
            out << " (approx. " << storm::utility::convertNumber<double>(value) << ")";
        }
    }
}

template<typename ValueType>
void printRange(std::ostream& out, ValueType const& min, ValueType const& max) {
    out << "[";
    if (min == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << min;
    }
    out << ", ";
    if (max == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << max;
    }
    out << "]";
    if (std::is_same<ValueType, storm::RationalNumber>::value) {
        out << " (approx. [";
        if (min == storm::utility::infinity<ValueType>()) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(min);
        }
        out << ", ";
        if (max == storm::utility::infinity<ValueType>()) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(max);
        }
        out << "])";
    }
    out << " (range)";
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::printValue(std::ostream& out, storm::storage::sparse::state_type state) const {
    print(out, (*this)[state]);
    if (!this->hasLowerBounds() && !this->hasUpperBounds()) {
        return;
    }
    // The enclosure is printed next to the estimate. Note that this is unrelated to the range over all states
    // that is printed for large results below.
    out << " [";
    if (this->hasLowerBounds()) {
        print(out, this->isResultForAllStates() ? this->getLowerBoundVector()[state] : this->getLowerBoundMap().at(state));
    } else {
        out << "-inf";
    }
    out << ", ";
    if (this->hasUpperBounds()) {
        print(out, this->isResultForAllStates() ? this->getUpperBoundVector()[state] : this->getUpperBoundMap().at(state));
    } else {
        out << "inf";
    }
    out << "]";
}

template<typename ValueType>
std::ostream& ExplicitQuantitativeCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    bool minMaxSupported = std::is_same<ValueType, double>::value || std::is_same<ValueType, storm::RationalNumber>::value;
    bool printAsRange = false;

    if (this->isResultForAllStates()) {
        vector_type const& valuesAsVector = boost::get<vector_type>(values);
        if (valuesAsVector.size() >= 10 && minMaxSupported) {
            printAsRange = true;
        } else {
            out << "{";
            bool first = true;
            for (uint64_t state = 0; state < valuesAsVector.size(); ++state) {
                if (!first) {
                    out << ", ";
                } else {
                    first = false;
                }
                this->printValue(out, state);
            }
            out << "}";
        }
    } else {
        map_type const& valuesAsMap = boost::get<map_type>(values);
        if (valuesAsMap.size() >= 10 && minMaxSupported) {
            printAsRange = true;
        } else {
            if (valuesAsMap.size() == 1) {
                this->printValue(out, valuesAsMap.begin()->first);
            } else {
                out << "{";
                bool first = true;
                for (auto const& element : valuesAsMap) {
                    if (!first) {
                        out << ", ";
                    } else {
                        first = false;
                    }
                    this->printValue(out, element.first);
                }
                out << "}";
            }
        }
    }

    if (printAsRange) {
        std::pair<ValueType, ValueType> minmax = this->getMinMax();
        printRange(out, minmax.first, minmax.second);
    }

    return out;
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<ValueType>::compareAgainstBound(storm::logic::ComparisonType comparisonType,
                                                                                             ValueType const& bound) const {
    if (this->isResultForAllStates()) {
        vector_type const& valuesAsVector = boost::get<vector_type>(values);
        storm::storage::BitVector result(valuesAsVector.size());
        switch (comparisonType) {
            case logic::ComparisonType::Less:
                for (uint_fast64_t index = 0; index < valuesAsVector.size(); ++index) {
                    if (valuesAsVector[index] < bound) {
                        result.set(index);
                    }
                }
                break;
            case logic::ComparisonType::LessEqual:
                for (uint_fast64_t index = 0; index < valuesAsVector.size(); ++index) {
                    if (valuesAsVector[index] <= bound) {
                        result.set(index);
                    }
                }
                break;
            case logic::ComparisonType::Greater:
                for (uint_fast64_t index = 0; index < valuesAsVector.size(); ++index) {
                    if (valuesAsVector[index] > bound) {
                        result.set(index);
                    }
                }
                break;
            case logic::ComparisonType::GreaterEqual:
                for (uint_fast64_t index = 0; index < valuesAsVector.size(); ++index) {
                    if (valuesAsVector[index] >= bound) {
                        result.set(index);
                    }
                }
                break;
        }
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ValueType>(std::move(result), std::move(scheduler)));
    } else {
        map_type const& valuesAsMap = boost::get<map_type>(values);
        std::map<storm::storage::sparse::state_type, bool> result;
        switch (comparisonType) {
            case logic::ComparisonType::Less:
                for (auto const& element : valuesAsMap) {
                    result[element.first] = element.second < bound;
                }
                break;
            case logic::ComparisonType::LessEqual:
                for (auto const& element : valuesAsMap) {
                    result[element.first] = element.second <= bound;
                }
                break;
            case logic::ComparisonType::Greater:
                for (auto const& element : valuesAsMap) {
                    result[element.first] = element.second > bound;
                }
                break;
            case logic::ComparisonType::GreaterEqual:
                for (auto const& element : valuesAsMap) {
                    result[element.first] = element.second >= bound;
                }
                break;
        }
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ValueType>(std::move(result), std::move(scheduler)));
    }
}

template<>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<storm::RationalFunction>::compareAgainstBound(storm::logic::ComparisonType comparisonType,
                                                                                                           storm::RationalFunction const& bound) const {
    // Since it is not possible to compare rational functions against bounds, we simply call the base class method.
    return QuantitativeCheckResult::compareAgainstBound(comparisonType, bound);
}

template<typename ValueType>
ValueType& ExplicitQuantitativeCheckResult<ValueType>::operator[](storm::storage::sparse::state_type state) {
    if (this->isResultForAllStates()) {
        return boost::get<vector_type>(values)[state];
    } else {
        return boost::get<map_type>(values)[state];
    }
}

template<typename ValueType>
ValueType const& ExplicitQuantitativeCheckResult<ValueType>::operator[](storm::storage::sparse::state_type state) const {
    if (this->isResultForAllStates()) {
        return boost::get<vector_type>(values)[state];
    } else {
        map_type const& valuesAsMap = boost::get<map_type>(values);
        auto const& keyValuePair = valuesAsMap.find(state);
        STORM_LOG_THROW(keyValuePair != valuesAsMap.end(), storm::exceptions::InvalidOperationException, "Unknown key '" << state << "'.");
        return keyValuePair->second;
    }
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isResultForAllStates() const {
    return values.which() == 0;
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isExplicitQuantitativeCheckResult() const {
    return true;
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::oneMinus() {
    detail::complementValues<ValueType>(values);
    if (this->hasLowerBounds()) {
        detail::complementValues<ValueType>(*lowerBounds);
    }
    if (this->hasUpperBounds()) {
        detail::complementValues<ValueType>(*upperBounds);
    }
    // One minus is antitone, so what used to bound the values from below now bounds them from above.
    std::swap(lowerBounds, upperBounds);
}

template<typename ValueType>
void insertJsonEntry(storm::json<ValueType>& json, uint64_t const& id, ValueType const& value,
                     std::optional<storm::storage::sparse::Valuations> const& stateValuations = std::nullopt,
                     std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt,
                     std::optional<ValueType> const& lowerBound = std::nullopt, std::optional<ValueType> const& upperBound = std::nullopt) {
    typename storm::json<ValueType> entry;
    if (stateValuations) {
        entry["s"] = stateValuations->template toJson<ValueType>(id);
    } else {
        entry["s"] = id;
    }
    entry["v"] = value;
    if (lowerBound) {
        entry["lb"] = *lowerBound;
    }
    if (upperBound) {
        entry["ub"] = *upperBound;
    }
    if (stateLabels) {
        auto labs = stateLabels->getLabelsOfState(id);
        entry["l"] = labs;
    }
    json.push_back(std::move(entry));
}

template<typename ValueType>
storm::json<ValueType> ExplicitQuantitativeCheckResult<ValueType>::toJson(std::optional<storm::storage::sparse::Valuations> const& stateValuations,
                                                                          std::optional<storm::models::sparse::StateLabeling> const& stateLabels) const {
    storm::json<ValueType> result;
    if (this->isResultForAllStates()) {
        vector_type const& valuesAsVector = boost::get<vector_type>(values);
        for (uint64_t state = 0; state < valuesAsVector.size(); ++state) {
            insertJsonEntry(result, state, valuesAsVector[state], stateValuations, stateLabels,
                            this->hasLowerBounds() ? std::make_optional(this->getLowerBoundVector()[state]) : std::nullopt,
                            this->hasUpperBounds() ? std::make_optional(this->getUpperBoundVector()[state]) : std::nullopt);
        }
    } else {
        map_type const& valuesAsMap = boost::get<map_type>(values);
        for (auto const& stateValue : valuesAsMap) {
            insertJsonEntry(result, stateValue.first, stateValue.second, stateValuations, stateLabels,
                            this->hasLowerBounds() ? std::make_optional(this->getLowerBoundMap().at(stateValue.first)) : std::nullopt,
                            this->hasUpperBounds() ? std::make_optional(this->getUpperBoundMap().at(stateValue.first)) : std::nullopt);
        }
    }
    return result;
}

template<>
storm::json<storm::RationalFunction> ExplicitQuantitativeCheckResult<storm::RationalFunction>::toJson(
    std::optional<storm::storage::sparse::Valuations> const&, std::optional<storm::models::sparse::StateLabeling> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Export of Check results is not supported for Rational Functions.");
}

template class ExplicitQuantitativeCheckResult<double>;
template class ExplicitQuantitativeCheckResult<storm::RationalNumber>;
template class ExplicitQuantitativeCheckResult<storm::RationalFunction>;
}  // namespace modelchecker
}  // namespace storm
