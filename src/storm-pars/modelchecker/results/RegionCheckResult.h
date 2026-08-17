#pragma once

#include <vector>

#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/modelchecker/results/CheckResult.h"

namespace storm {
namespace modelchecker {
template<typename ValueType>
class RegionCheckResult : public CheckResult {
   public:
    RegionCheckResult(std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>> const& regionResults);
    RegionCheckResult(std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>>&& regionResults);
    virtual ~RegionCheckResult() = default;

    virtual bool isRegionCheckResult() const;
    virtual bool isRegionRefinementCheckResult() const;

    std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>> const& getRegionResults() const;
    typename storm::storage::ParameterRegion<ValueType>::CoefficientType const& getSatFraction() const;
    typename storm::storage::ParameterRegion<ValueType>::CoefficientType const& getUnsatFraction() const;
    typename storm::storage::ParameterRegion<ValueType>::CoefficientType const& getIllDefinedFraction() const;

    virtual std::ostream& writeToStream(std::ostream& out) const override;
    virtual std::ostream& writeCondensedToStream(std::ostream& out) const;
    virtual std::ostream& writeIllustrationToStream(std::ostream& out) const;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual std::unique_ptr<CheckResult> clone() const override;

   protected:
    virtual void initFractions(typename storm::storage::ParameterRegion<ValueType>::CoefficientType const& overallArea);

    std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>> regionResults;
    typename storm::storage::ParameterRegion<ValueType>::CoefficientType satFraction, unsatFraction, illDefinedFraction;
};
}  // namespace modelchecker
}  // namespace storm
