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
#include "storm/storage/sparse/StateValuations.h"

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

    storm::json<ValueType> toJson(std::optional<storm::storage::sparse::StateValuations> const& stateValuations = std::nullopt,
                                  std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt) const;

   private:
    bool hasValueType(std::type_info const& t) const override {
        return t == typeid(ValueType);
    }

    // The values of the quantitative check result.
    boost::variant<vector_type, map_type> values;

    // An optional scheduler that accompanies the values.
    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler;
};
}  // namespace modelchecker
}  // namespace storm
