#pragma once
#include <optional>
#include <vector>

#include "storm/adapters/JsonForward.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/solver/SolutionBounds.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/storage/valuations/Valuations.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {

namespace modelchecker {
// Forward declaration
template<typename ValueType>
class ExplicitQualitativeCheckResult;

/*!
 * A quantitative check result over the states of a sparse model.
 *
 * The result either covers every state of the model or just a subset of them, e.g. after filtering it to the
 * initial states. In the latter case the states in question are recorded in a bit vector and the values are
 * stored compressed, i.e. the i-th value belongs to the i-th state selected by that bit vector.
 */
template<typename ValueType>
class ExplicitQuantitativeCheckResult : public QuantitativeCheckResult<ValueType> {
   public:
    typedef typename QuantitativeCheckResult<ValueType>::ExtendedValueType ExtendedValueType;
    typedef std::vector<ExtendedValueType> vector_type;

    /*!
     * Creates a result that holds the given value for the given state only.
     */
    ExplicitQuantitativeCheckResult(storm::storage::sparse::state_type const& state, ExtendedValueType const& value);

    /*!
     * Creates a result for all states of a model with the given number of states.
     */
    ExplicitQuantitativeCheckResult(vector_type const& values, std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});
    ExplicitQuantitativeCheckResult(vector_type&& values, std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    /*!
     * Creates a result for the given states only.
     * @param states The states the result is for.
     * @param values One value per state selected by @p states, in the order of the selected states.
     */
    ExplicitQuantitativeCheckResult(storm::storage::BitVector states, vector_type&& values,
                                    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    /*!
     * Takes over values that are still expressed in the plain value type, which are taken to be finite. A
     * computation that can produce an infinite value hands over the extended type instead.
     */
    ExplicitQuantitativeCheckResult(std::vector<ValueType> const& values)
        requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>);
    ExplicitQuantitativeCheckResult(std::vector<ValueType>&& values)
        requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>);
    ExplicitQuantitativeCheckResult(storm::storage::BitVector states, std::vector<ValueType>&& values)
        requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>);

    ExplicitQuantitativeCheckResult(ExplicitQuantitativeCheckResult const& other) = default;
    ExplicitQuantitativeCheckResult& operator=(ExplicitQuantitativeCheckResult const& other) = default;
    ExplicitQuantitativeCheckResult(ExplicitQuantitativeCheckResult&& other) = default;
    ExplicitQuantitativeCheckResult& operator=(ExplicitQuantitativeCheckResult&& other) = default;
    explicit ExplicitQuantitativeCheckResult(ExplicitQualitativeCheckResult<ValueType> const& other);

    virtual ~ExplicitQuantitativeCheckResult() = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    /*!
     * Retrieves the value of the given state.
     * @pre The result holds a value for that state.
     */
    ExtendedValueType& operator[](storm::storage::sparse::state_type state);
    ExtendedValueType const& operator[](storm::storage::sparse::state_type state) const;

    virtual std::unique_ptr<CheckResult> compareAgainstBound(storm::logic::ComparisonType comparisonType, ValueType const& bound) const override;

    virtual bool isExplicit() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isExplicitQuantitativeCheckResult() const override;

    /*!
     * Retrieves whether the result holds a value for the given state.
     */
    bool hasValueForState(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves the states this result holds values for.
     * @pre This is not a result for all states.
     */
    storm::storage::BitVector const& getStates() const;

    /*!
     * Retrieves the values, one per state this result is for. If this is not a result for all states, the i-th
     * value belongs to the i-th state selected by getStates().
     */
    vector_type const& getValueVector() const;
    vector_type& getValueVector();

    /*!
     * @pre no value is infinite
     * @return the values, narrowed back to the plain value type.
     */
    std::vector<ValueType> getFiniteValueVector() const;

    /*!
     * @return the values, with every infinite one written as the value that storm::utility::infinity yields for the
     * plain value type.
     */
    std::vector<ValueType> getSentinelValueVector() const;

    /*!
     * Retrieves whether sound lower resp. upper bounds on the actual values are known.
     */
    bool hasLowerBounds() const;
    bool hasUpperBounds() const;

    /*!
     * Retrieves the sound lower resp. upper bounds on the actual values. These have the same shape as the
     * values, i.e. they are indexed in the same way.
     * @pre The respective bounds are known.
     */
    vector_type const& getLowerBoundVector() const;
    vector_type const& getUpperBoundVector() const;

    /*!
     * Retrieves both bounds at once, either of which may be unset.
     */
    storm::solver::SolutionBounds<ExtendedValueType> const& getSolutionBounds() const;

    /*!
     * Sets sound bounds on the actual values. Each bound must have the same shape as the values, i.e. hold one
     * entry per state this result is for.
     */
    void setLowerBounds(vector_type lowerBounds);
    void setUpperBounds(vector_type upperBounds);
    void setBounds(storm::solver::SolutionBounds<ExtendedValueType> bounds);

    /*!
     * Sets bounds that are still expressed in the plain value type, which are taken to be finite. An algorithm
     * that can bound a value by infinity hands over the extended type instead.
     */
    void setBounds(storm::solver::SolutionBounds<ValueType> bounds)
        requires(!std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>);

    /*!
     * Drops all bounds, e.g. after an operation that cannot maintain them.
     */
    void clearBounds();

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual void oneMinus() override;

    virtual ExtendedValueType getMin() const override;
    virtual ExtendedValueType getMax() const override;
    virtual std::pair<ExtendedValueType, ExtendedValueType> getMinMax() const;
    virtual ExtendedValueType average() const override;
    virtual ExtendedValueType sum() const override;

    virtual AggregatedValue<ExtendedValueType> aggregate(FilterType filter) const override;

    virtual bool hasScheduler() const override;
    void setScheduler(std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler);
    storm::storage::Scheduler<ValueType> const& getScheduler() const;
    storm::storage::Scheduler<ValueType>& getScheduler();

    storm::json<ValueType> toJson(std::optional<storm::storage::sparse::Valuations> const& stateValuations = std::nullopt,
                                  std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt) const;

   private:
    bool hasValueType(std::type_info const& t) const override {
        return t == typeid(ValueType);
    }

    /*!
     * Aggregates a single vector of this result, i.e. the values or one of the bounds on them.
     */
    static ExtendedValueType aggregateVector(vector_type const& vector, FilterType filter);

    /*!
     * Retrieves the index at which the value of the given state is stored.
     * @pre The result holds a value for that state.
     */
    uint64_t getOffset(storm::storage::sparse::state_type state) const;

    /*!
     * Invokes the given function with the state and the offset of its value, for every state this result is for.
     */
    template<typename Function>
    void forEachState(Function const& f) const {
        if (states) {
            uint64_t offset = 0;
            for (auto const& state : *states) {
                f(state, offset);
                ++offset;
            }
        } else {
            for (uint64_t state = 0; state < values.size(); ++state) {
                f(state, state);
            }
        }
    }

    /*!
     * Asserts that the given bounds have the same shape as the values.
     */
    void assertBoundsShape(vector_type const& bounds) const;

    /*!
     * Writes the value stored at the given offset, followed by its bounds if any are known.
     */
    void printValue(std::ostream& out, uint64_t offset) const;

    // The states this result holds values for, or nothing at all if it is a result for all states.
    std::optional<storm::storage::BitVector> states;

    // The values of the quantitative check result, one per state this result is for. These are estimates that
    // lie within the bounds below but carry no further guarantee.
    vector_type values;

    // Sound bounds on the actual values, if an algorithm provided them.
    storm::solver::SolutionBounds<ExtendedValueType> bounds;

    // An optional scheduler that accompanies the values.
    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler;
};
}  // namespace modelchecker
}  // namespace storm
