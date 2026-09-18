#include "storm/adapters/RationalNumberAdapter.h"  // Must come first. TODO: fix

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(storm::storage::sparse::state_type const& state, ExtendedValueType const& value)
    : states(storm::storage::BitVector(state + 1)), values({value}) {
    states->set(state);
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(vector_type const& values,
                                                                            std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : values(values), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(vector_type&& values,
                                                                            std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : values(std::move(values)), scheduler(scheduler) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(storm::storage::BitVector states, vector_type&& values,
                                                                            std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler)
    : states(std::move(states)), values(std::move(values)), scheduler(scheduler) {
    STORM_LOG_ASSERT(this->states->getNumberOfSetBits() == this->values.size(), "Expected one value per selected state.");
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(std::vector<ValueType> const& values)
    requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
    : values(storm::utility::widen(std::vector<ValueType>(values))) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(std::vector<ValueType>&& values)
    requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
    : values(storm::utility::widen(std::move(values))) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(storm::storage::BitVector states, std::vector<ValueType>&& values)
    requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
    : states(std::move(states)), values(storm::utility::widen(std::move(values))) {
    STORM_LOG_ASSERT(this->states->getNumberOfSetBits() == this->values.size(), "Expected one value per selected state.");
}

template<typename ValueType>
ExplicitQuantitativeCheckResult<ValueType>::ExplicitQuantitativeCheckResult(ExplicitQualitativeCheckResult<ValueType> const& other) {
    auto const toValue = [](bool truthValue) { return truthValue ? storm::utility::one<ExtendedValueType>() : storm::utility::zero<ExtendedValueType>(); };

    storm::storage::BitVector const& truthValues = other.getTruthValuesVector();
    values.reserve(truthValues.size());
    for (std::size_t i = 0, n = truthValues.size(); i < n; i++) {
        values.push_back(toValue(truthValues.get(i)));
    }
    if (!other.isResultForAllStates()) {
        states = other.getStates();
    }
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<ValueType>::clone() const {
    return std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>(*this);
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasValueForState(storm::storage::sparse::state_type state) const {
    if (states) {
        return state < states->size() && states->get(state);
    }
    return state < values.size();
}

template<typename ValueType>
uint64_t ExplicitQuantitativeCheckResult<ValueType>::getOffset(storm::storage::sparse::state_type state) const {
    STORM_LOG_THROW(this->hasValueForState(state), storm::exceptions::InvalidOperationException, "Unknown key '" << state << "'.");
    return states ? states->getNumberOfSetBitsBeforeIndex(state) : state;
}

template<typename ValueType>
storm::storage::BitVector const& ExplicitQuantitativeCheckResult<ValueType>::getStates() const {
    STORM_LOG_THROW(!this->isResultForAllStates(), storm::exceptions::InvalidOperationException,
                    "Unable to retrieve the states of a result that is for all states.");
    return *states;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getValueVector() const {
    return values;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type& ExplicitQuantitativeCheckResult<ValueType>::getValueVector() {
    return values;
}

template<typename ValueType>
std::vector<ValueType> ExplicitQuantitativeCheckResult<ValueType>::getFiniteValueVector() const {
    return storm::utility::narrowFinite<ValueType>(values);
}

template<typename ValueType>
std::vector<ValueType> ExplicitQuantitativeCheckResult<ValueType>::getSentinelValueVector() const {
    if constexpr (!std::is_same_v<ExtendedValueType, ValueType>) {
        std::vector<ValueType> result;
        result.reserve(values.size());
        for (auto const& value : values) {
            result.push_back(storm::utility::toSentinel<ValueType>(value));
        }
        return result;
    } else {
        return values;
    }
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasLowerBounds() const {
    return bounds.hasLower();
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::hasUpperBounds() const {
    return bounds.hasUpper();
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getLowerBoundVector() const {
    STORM_LOG_THROW(this->hasLowerBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown lower bounds.");
    return *bounds.lower;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::vector_type const& ExplicitQuantitativeCheckResult<ValueType>::getUpperBoundVector() const {
    STORM_LOG_THROW(this->hasUpperBounds(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown upper bounds.");
    return *bounds.upper;
}

template<typename ValueType>
storm::solver::SolutionBounds<typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType> const&
ExplicitQuantitativeCheckResult<ValueType>::getSolutionBounds() const {
    return bounds;
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::assertBoundsShape([[maybe_unused]] vector_type const& bounds) const {
    STORM_LOG_ASSERT(bounds.size() == values.size(), "Bounds must have the same size as the values.");
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setLowerBounds(vector_type lowerBounds) {
    this->assertBoundsShape(lowerBounds);
    this->bounds.lower = std::move(lowerBounds);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setUpperBounds(vector_type upperBounds) {
    this->assertBoundsShape(upperBounds);
    this->bounds.upper = std::move(upperBounds);
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setBounds(storm::solver::SolutionBounds<ExtendedValueType> bounds) {
    if (bounds.hasLower()) {
        this->setLowerBounds(std::move(*bounds.lower));
    }
    if (bounds.hasUpper()) {
        this->setUpperBounds(std::move(*bounds.upper));
    }
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::setBounds(storm::solver::SolutionBounds<ValueType> bounds)
    requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
{
    if (bounds.hasLower()) {
        this->setLowerBounds(storm::utility::widen(std::move(*bounds.lower)));
    }
    if (bounds.hasUpper()) {
        this->setUpperBounds(storm::utility::widen(std::move(*bounds.upper)));
    }
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::clearBounds() {
    bounds.clear();
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    STORM_LOG_THROW(filter.hasValueType<ValueType>(), storm::exceptions::InvalidOperationException, "Filter has unexpected value type.");
    ExplicitQualitativeCheckResult<ValueType> const& explicitFilter = filter.template asExplicitQualitativeCheckResult<ValueType>();
    typename ExplicitQualitativeCheckResult<ValueType>::vector_type const& filterTruthValues = explicitFilter.getTruthValuesVector();

    // Line the filter up with the states this result has values for. The two need not span the same range of
    // states, e.g. if this result holds a value for a single state only.
    uint64_t const numStates = std::max(filterTruthValues.size(), states ? states->size() : values.size());
    storm::storage::BitVector available = states ? *states : storm::storage::BitVector(values.size(), true);
    available.resize(numStates);
    storm::storage::BitVector selected(filterTruthValues);
    selected.resize(numStates);
    STORM_LOG_THROW(selected.isSubsetOf(available), storm::exceptions::InvalidOperationException,
                    "The check result fails to contain some results referred to by the filter.");

    storm::storage::BitVector const keep = selected % available;

    if (this->hasLowerBounds()) {
        bounds.lower = storm::utility::vector::filterVector(*bounds.lower, keep);
    }
    if (this->hasUpperBounds()) {
        bounds.upper = storm::utility::vector::filterVector(*bounds.upper, keep);
    }
    values = storm::utility::vector::filterVector(values, keep);
    states = filterTruthValues;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType ExplicitQuantitativeCheckResult<ValueType>::getMin() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Minimum of empty set is not defined.");
    return storm::utility::minimum(values);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType ExplicitQuantitativeCheckResult<ValueType>::getMax() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Maximum of empty set is not defined.");
    return storm::utility::maximum(values);
}

template<typename ValueType>
std::pair<typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType, typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType>
ExplicitQuantitativeCheckResult<ValueType>::getMinMax() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Minimum/maximum of empty set is not defined.");
    return storm::utility::minmax(values);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType ExplicitQuantitativeCheckResult<ValueType>::sum() const {
    STORM_LOG_THROW(!values.empty(), storm::exceptions::InvalidOperationException, "Sum of empty set is not defined.");

    ExtendedValueType sum = storm::utility::zero<ExtendedValueType>();
    for (auto const& element : values) {
        STORM_LOG_THROW(!storm::utility::isInfinity(element), storm::exceptions::InvalidOperationException,
                        "Cannot compute the sum of values containing infinity.");
        sum += element;
    }
    return sum;
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType ExplicitQuantitativeCheckResult<ValueType>::average() const {
    return this->sum() / storm::utility::convertNumber<ExtendedValueType, uint64_t>(values.size());
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType ExplicitQuantitativeCheckResult<ValueType>::aggregateVector(vector_type const& vector,
                                                                                                                                   FilterType filter) {
    // A bound has the same shape as the values, so wrapping it in a result of its own reuses the methods above
    // rather than repeating them.
    ExplicitQuantitativeCheckResult<ValueType> const asResult{vector};
    switch (filter) {
        case FilterType::MIN:
            return asResult.getMin();
        case FilterType::MAX:
            return asResult.getMax();
        case FilterType::SUM:
            return asResult.sum();
        case FilterType::AVG:
            return asResult.average();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "The filter " << toString(filter) << " does not aggregate values.");
    }
}

template<typename ValueType>
AggregatedValue<typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType> ExplicitQuantitativeCheckResult<ValueType>::aggregate(
    FilterType filter) const {
    // The values go through the base implementation, so the aggregations keep what getMin(), sum() and friends
    // promise about them.
    AggregatedValue<ExtendedValueType> result = QuantitativeCheckResult<ValueType>::aggregate(filter);
    if (this->hasLowerBounds()) {
        result.lower = aggregateVector(*bounds.lower, filter);
    }
    if (this->hasUpperBounds()) {
        result.upper = aggregateVector(*bounds.upper, filter);
    }
    return result;
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
    if (storm::utility::isInfinity(value)) {
        out << "inf";
    } else {
        out << value;
        if (std::is_same_v<ValueType, storm::RationalNumber> || std::is_same_v<ValueType, storm::ExtendedRationalNumber>) {
            out << " (approx. " << storm::utility::convertNumber<double>(value) << ")";
        }
    }
}

template<typename ValueType>
void printRange(std::ostream& out, ValueType const& min, ValueType const& max) {
    out << "[";
    print(out, min);
    out << ", ";
    print(out, max);
    out << "]";
    if (std::is_same_v<ValueType, storm::RationalNumber> || std::is_same_v<ValueType, storm::ExtendedRationalNumber>) {
        out << " (approx. [";
        if (storm::utility::isInfinity(min)) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(min);
        }
        out << ", ";
        if (storm::utility::isInfinity(max)) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(max);
        }
        out << "])";
    }
    out << " (range)";
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::printValue(std::ostream& out, uint64_t offset) const {
    print(out, values[offset]);
    if (!this->hasLowerBounds() && !this->hasUpperBounds()) {
        return;
    }
    out << " [";
    // A side that is not known is written as a dash, so that it cannot be read as an infinite bound.
    if (this->hasLowerBounds()) {
        print(out, this->getLowerBoundVector()[offset]);
    } else {
        out << "-";
    }
    out << ", ";
    if (this->hasUpperBounds()) {
        print(out, this->getUpperBoundVector()[offset]);
    } else {
        out << "-";
    }
    out << "]";
}

template<typename ValueType>
std::ostream& ExplicitQuantitativeCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    bool minMaxSupported = std::is_same<ValueType, double>::value || std::is_same<ValueType, storm::RationalNumber>::value;

    if (values.size() >= 10 && minMaxSupported) {
        std::pair<ExtendedValueType, ExtendedValueType> minmax = this->getMinMax();
        printRange(out, minmax.first, minmax.second);
    } else if (!this->isResultForAllStates() && values.size() == 1) {
        this->printValue(out, 0);
    } else {
        out << "{";
        for (uint64_t offset = 0; offset < values.size(); ++offset) {
            if (offset > 0) {
                out << ", ";
            }
            this->printValue(out, offset);
        }
        out << "}";
    }

    return out;
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<ValueType>::compareAgainstBound(storm::logic::ComparisonType comparisonType,
                                                                                             ValueType const& bound) const {
    auto const compare = [&comparisonType, &bound](ExtendedValueType const& value) {
        switch (comparisonType) {
            case logic::ComparisonType::Less:
                return value < bound;
            case logic::ComparisonType::LessEqual:
                return value <= bound;
            case logic::ComparisonType::Greater:
                return value > bound;
            case logic::ComparisonType::GreaterEqual:
                return value >= bound;
        }
        return false;
    };

    // A comparison is only sound if the bound falls outside the lower and upper bound of the result.
    if (this->hasLowerBounds() && this->hasUpperBounds()) {
        for (uint64_t offset = 0; offset < values.size(); ++offset) {
            STORM_LOG_WARN_COND(!((*bounds.lower)[offset] < bound && bound < (*bounds.upper)[offset]),
                                "The bound " << bound << " lies between the lower bound " << (*bounds.lower)[offset] << " and the upper bound "
                                             << (*bounds.upper)[offset] << ", so the comparison against it is not decided at state " << offset << ".");
        }
    }

    storm::storage::BitVector result(values.size());
    for (uint64_t offset = 0; offset < values.size(); ++offset) {
        if (compare(values[offset])) {
            result.set(offset);
        }
    }
    if (this->isResultForAllStates()) {
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ValueType>(std::move(result), scheduler));
    } else {
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ValueType>(*states, std::move(result), scheduler));
    }
}

template<>
std::unique_ptr<CheckResult> ExplicitQuantitativeCheckResult<storm::RationalFunction>::compareAgainstBound(storm::logic::ComparisonType comparisonType,
                                                                                                           storm::RationalFunction const& bound) const {
    // Since it is not possible to compare rational functions against bounds, we simply call the base class method.
    return QuantitativeCheckResult::compareAgainstBound(comparisonType, bound);
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType& ExplicitQuantitativeCheckResult<ValueType>::operator[](
    storm::storage::sparse::state_type state) {
    return values[this->getOffset(state)];
}

template<typename ValueType>
typename ExplicitQuantitativeCheckResult<ValueType>::ExtendedValueType const& ExplicitQuantitativeCheckResult<ValueType>::operator[](
    storm::storage::sparse::state_type state) const {
    return values[this->getOffset(state)];
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isResultForAllStates() const {
    return !states.has_value();
}

template<typename ValueType>
bool ExplicitQuantitativeCheckResult<ValueType>::isExplicitQuantitativeCheckResult() const {
    return true;
}

template<typename ValueType>
void ExplicitQuantitativeCheckResult<ValueType>::oneMinus() {
    storm::utility::vector::subtractFromConstantOneVector(values);
    if (this->hasLowerBounds()) {
        storm::utility::vector::subtractFromConstantOneVector(*bounds.lower);
    }
    if (this->hasUpperBounds()) {
        storm::utility::vector::subtractFromConstantOneVector(*bounds.upper);
    }
    // Inverse the bounds, lb = 1 - ub and ub = 1 - lb.
    std::swap(bounds.lower, bounds.upper);
}

/*!
 * Writes the given value under the given key, spelling out an infinity that the plain value type cannot represent.
 */
template<typename ValueType>
void insertJsonValue(storm::json<ValueType>& entry, std::string const& key, storm::utility::ExtendedValueType<ValueType> const& value) {
    if (storm::utility::isInfinity(value)) {
        entry[key] = "inf";
    } else if (storm::utility::isNegativeInfinity(value)) {
        entry[key] = "-inf";
    } else if constexpr (std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>) {
        entry[key] = value;
    } else {
        entry[key] = value.getFinite();
    }
}

template<typename ValueType>
void insertJsonEntry(storm::json<ValueType>& json, uint64_t const& id, storm::utility::ExtendedValueType<ValueType> const& value,
                     std::optional<storm::storage::sparse::Valuations> const& stateValuations = std::nullopt,
                     std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt,
                     std::optional<storm::utility::ExtendedValueType<ValueType>> const& lowerBound = std::nullopt,
                     std::optional<storm::utility::ExtendedValueType<ValueType>> const& upperBound = std::nullopt) {
    typename storm::json<ValueType> entry;
    if (stateValuations) {
        entry["s"] = stateValuations->template toJson<ValueType>(id);
    } else {
        entry["s"] = id;
    }
    insertJsonValue(entry, "v", value);
    if (lowerBound) {
        insertJsonValue(entry, "lb", *lowerBound);
    }
    if (upperBound) {
        insertJsonValue(entry, "ub", *upperBound);
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
    this->forEachState([&](storm::storage::sparse::state_type state, uint64_t offset) {
        insertJsonEntry(result, state, values[offset], stateValuations, stateLabels,
                        this->hasLowerBounds() ? std::make_optional(this->getLowerBoundVector()[offset]) : std::nullopt,
                        this->hasUpperBounds() ? std::make_optional(this->getUpperBoundVector()[offset]) : std::nullopt);
    });
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
