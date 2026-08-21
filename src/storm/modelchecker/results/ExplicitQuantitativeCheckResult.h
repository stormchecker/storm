#pragma once
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <map>
#include <optional>
#include <vector>

#include "storm/adapters/JsonForward.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/storage/valuations/Valuations.h"

namespace storm {

namespace modelchecker {
// Forward declaration
template<typename ValueType>
class ExplicitQualitativeCheckResult;

template<typename ValueType>
class ExplicitQuantitativeCheckResult : public QuantitativeCheckResult<ValueType> {
   public:
    typedef std::vector<ValueType> vector_type;
    typedef std::map<storm::storage::sparse::state_type, ValueType> map_type;

    ExplicitQuantitativeCheckResult();
    ExplicitQuantitativeCheckResult(map_type const& values);
    ExplicitQuantitativeCheckResult(map_type&& values);
    ExplicitQuantitativeCheckResult(storm::storage::sparse::state_type const& state, ValueType const& value);
    ExplicitQuantitativeCheckResult(vector_type const& values);
    ExplicitQuantitativeCheckResult(vector_type&& values);
    ExplicitQuantitativeCheckResult(boost::variant<vector_type, map_type> const& values,
                                    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});
    ExplicitQuantitativeCheckResult(boost::variant<vector_type, map_type>&& values,
                                    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    ExplicitQuantitativeCheckResult(ExplicitQuantitativeCheckResult const& other) = default;
    ExplicitQuantitativeCheckResult& operator=(ExplicitQuantitativeCheckResult const& other) = default;
    ExplicitQuantitativeCheckResult(ExplicitQuantitativeCheckResult&& other) = default;
    ExplicitQuantitativeCheckResult& operator=(ExplicitQuantitativeCheckResult&& other) = default;
    explicit ExplicitQuantitativeCheckResult(ExplicitQualitativeCheckResult<ValueType> const& other);

    virtual ~ExplicitQuantitativeCheckResult() = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    ValueType& operator[](storm::storage::sparse::state_type state);
    ValueType const& operator[](storm::storage::sparse::state_type state) const;

    virtual std::unique_ptr<CheckResult> compareAgainstBound(storm::logic::ComparisonType comparisonType, ValueType const& bound) const override;

    virtual bool isExplicit() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isExplicitQuantitativeCheckResult() const override;

    vector_type const& getValueVector() const;
    vector_type& getValueVector();
    map_type const& getValueMap() const;

    /*!
     * Retrieves whether sound lower resp. upper bounds on the actual values are known.
     * A bound that is not known is not stored at all, so that "no information" cannot be confused with an
     * infinite bound: an individual entry of a known bound may well be (minus) infinity.
     */
    bool hasLowerBounds() const;
    bool hasUpperBounds() const;

    /*!
     * Retrieves the sound lower resp. upper bounds on the actual values.
     * @pre The respective bounds are known.
     */
    vector_type const& getLowerBoundVector() const;
    vector_type const& getUpperBoundVector() const;
    map_type const& getLowerBoundMap() const;
    map_type const& getUpperBoundMap() const;

    /*!
     * Sets sound bounds on the actual values. The bounds must have the same shape as the values, i.e. a vector
     * of the same size resp. a map with the same keys.
     */
    void setLowerBounds(boost::variant<vector_type, map_type> lowerBounds);
    void setUpperBounds(boost::variant<vector_type, map_type> upperBounds);
    void setBounds(boost::variant<vector_type, map_type> lowerBounds, boost::variant<vector_type, map_type> upperBounds);

    /*!
     * Drops all bounds, e.g. after an operation that cannot maintain them.
     */
    void clearBounds();

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual void oneMinus() override;

    virtual ValueType getMin() const override;
    virtual ValueType getMax() const override;
    virtual std::pair<ValueType, ValueType> getMinMax() const;
    virtual ValueType average() const override;
    virtual ValueType sum() const override;

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
     * Asserts that the given bounds have the same shape as the values.
     */
    void assertBoundsShape(boost::variant<vector_type, map_type> const& bounds) const;

    /*!
     * Writes the value of the given state, followed by its bounds if any are known.
     */
    void printValue(std::ostream& out, storm::storage::sparse::state_type state) const;

    // The values of the quantitative check result. These are estimates of the actual values, which lie within
    // the bounds below but carry no further guarantee.
    boost::variant<vector_type, map_type> values;

    // Sound lower bounds on the actual values, if an algorithm provided them.
    std::optional<boost::variant<vector_type, map_type>> lowerBounds;

    // Sound upper bounds on the actual values, if an algorithm provided them.
    std::optional<boost::variant<vector_type, map_type>> upperBounds;

    // An optional scheduler that accompanies the values.
    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler;
};
}  // namespace modelchecker
}  // namespace storm
