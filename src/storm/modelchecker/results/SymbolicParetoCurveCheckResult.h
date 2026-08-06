#pragma once

#include <vector>

#include "storm/modelchecker/results/ParetoCurveCheckResult.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type, typename ValueType = double>
class SymbolicParetoCurveCheckResult : public ParetoCurveCheckResult<ValueType> {
   public:
    SymbolicParetoCurveCheckResult();
    SymbolicParetoCurveCheckResult(storm::dd::Bdd<Type> const& state, std::vector<typename ParetoCurveCheckResult<ValueType>::point_type> const& points,
                                   typename ParetoCurveCheckResult<ValueType>::polytope_type const& underApproximation,
                                   typename ParetoCurveCheckResult<ValueType>::polytope_type const& overApproximation);
    SymbolicParetoCurveCheckResult(storm::dd::Bdd<Type> const& state, std::vector<typename ParetoCurveCheckResult<ValueType>::point_type>&& points,
                                   typename ParetoCurveCheckResult<ValueType>::polytope_type&& underApproximation,
                                   typename ParetoCurveCheckResult<ValueType>::polytope_type&& overApproximation);

    SymbolicParetoCurveCheckResult(SymbolicParetoCurveCheckResult const& other) = default;
    SymbolicParetoCurveCheckResult& operator=(SymbolicParetoCurveCheckResult const& other) = default;
    SymbolicParetoCurveCheckResult(SymbolicParetoCurveCheckResult&& other) = default;
    SymbolicParetoCurveCheckResult& operator=(SymbolicParetoCurveCheckResult&& other) = default;
    virtual ~SymbolicParetoCurveCheckResult() = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    virtual bool isSymbolicParetoCurveCheckResult() const override;
    virtual bool isSymbolic() const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    storm::dd::Bdd<Type> const& getState() const;

   private:
    // The state of the checked model to which the result applies
    storm::dd::Bdd<Type> state;
};
}  // namespace modelchecker
}  // namespace storm
