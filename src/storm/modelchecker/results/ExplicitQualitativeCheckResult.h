#pragma once

#include <boost/variant.hpp>
#include <map>
#include <optional>

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/JsonForward.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/storage/sparse/StateValuations.h"

namespace storm {

namespace modelchecker {

template<typename ValueType>
class ExplicitQualitativeCheckResult : public QualitativeCheckResult {
   public:
    typedef storm::storage::BitVector vector_type;
    typedef std::map<storm::storage::sparse::state_type, bool> map_type;

    ExplicitQualitativeCheckResult();
    virtual ~ExplicitQualitativeCheckResult() = default;
    ExplicitQualitativeCheckResult(map_type const& map);
    ExplicitQualitativeCheckResult(map_type&& map);
    ExplicitQualitativeCheckResult(storm::storage::sparse::state_type state, bool value);
    ExplicitQualitativeCheckResult(vector_type const& truthValues);
    ExplicitQualitativeCheckResult(vector_type&& truthValues);
    ExplicitQualitativeCheckResult(boost::variant<vector_type, map_type> const& truthValues,
                                   std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});
    ExplicitQualitativeCheckResult(boost::variant<vector_type, map_type>&& truthValues,
                                   std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    ExplicitQualitativeCheckResult(ExplicitQualitativeCheckResult const& other) = default;
    ExplicitQualitativeCheckResult& operator=(ExplicitQualitativeCheckResult const& other) = default;
    ExplicitQualitativeCheckResult(ExplicitQualitativeCheckResult&& other) = default;
    ExplicitQualitativeCheckResult& operator=(ExplicitQualitativeCheckResult&& other) = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    bool operator[](storm::storage::sparse::state_type index) const;

    virtual bool isExplicit() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isExplicitQualitativeCheckResult() const override;

    virtual QualitativeCheckResult& operator&=(QualitativeCheckResult const& other) override;
    virtual QualitativeCheckResult& operator|=(QualitativeCheckResult const& other) override;
    virtual void complement() override;

    vector_type const& getTruthValuesVector() const;
    map_type const& getTruthValuesMap() const;

    virtual bool existsTrue() const override;
    virtual bool forallTrue() const override;
    virtual uint64_t count() const override;

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual bool hasScheduler() const override;
    void setScheduler(std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler);
    storm::storage::Scheduler<ValueType> const& getScheduler() const;
    storm::storage::Scheduler<ValueType>& getScheduler();

    template<typename JsonRationalType>
    storm::json<JsonRationalType> toJson(std::optional<storm::storage::sparse::StateValuations> const& stateValuations = std::nullopt,
                                         std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt) const;

   private:
    static void performLogicalOperation(ExplicitQualitativeCheckResult& first, QualitativeCheckResult const& second, bool logicalAnd);

    // The values of the quantitative check result.
    boost::variant<vector_type, map_type> truthValues;

    // An optional scheduler that accompanies the values.
    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler;
};
}  // namespace modelchecker
}  // namespace storm
