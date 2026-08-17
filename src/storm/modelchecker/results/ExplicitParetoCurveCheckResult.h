#pragma once

#include <vector>

#include "storm/modelchecker/results/ParetoCurveCheckResult.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace modelchecker {
template<typename ValueType>
class ExplicitParetoCurveCheckResult : public ParetoCurveCheckResult<ValueType> {
   public:
    using point_type = typename ParetoCurveCheckResult<ValueType>::point_type;
    using polytope_type = typename ParetoCurveCheckResult<ValueType>::polytope_type;
    using scheduler_type = storm::storage::Scheduler<ValueType>;
    ExplicitParetoCurveCheckResult();
    ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state, std::vector<point_type> const& points,
                                   polytope_type const& underApproximation = nullptr, polytope_type const& overApproximation = nullptr);
    ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state, std::vector<point_type>&& points,
                                   polytope_type&& underApproximation = nullptr, polytope_type&& overApproximation = nullptr);
    ExplicitParetoCurveCheckResult(storm::storage::sparse::state_type const& state, std::vector<point_type>&& points, std::vector<scheduler_type>&& schedulers,
                                   polytope_type&& underApproximation = nullptr, polytope_type&& overApproximation = nullptr);

    ExplicitParetoCurveCheckResult(ExplicitParetoCurveCheckResult const& other) = default;
    ExplicitParetoCurveCheckResult& operator=(ExplicitParetoCurveCheckResult const& other) = default;
    ExplicitParetoCurveCheckResult(ExplicitParetoCurveCheckResult&& other) = default;
    ExplicitParetoCurveCheckResult& operator=(ExplicitParetoCurveCheckResult&& other) = default;
    virtual ~ExplicitParetoCurveCheckResult() = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    virtual bool isExplicitParetoCurveCheckResult() const override;
    virtual bool isExplicit() const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    storm::storage::sparse::state_type const& getState() const;

    virtual bool hasScheduler() const override;

    std::vector<scheduler_type> const& getSchedulers() const;
    std::vector<scheduler_type>& getSchedulers();

   private:
    // The state of the checked model to which the result applies
    storm::storage::sparse::state_type state;
    // The corresponding strategies to reach each point in the pareto curve
    std::vector<scheduler_type> schedulers;
};
}  // namespace modelchecker
}  // namespace storm
