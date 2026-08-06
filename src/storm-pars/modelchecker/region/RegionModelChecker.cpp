#include "storm-pars/modelchecker/region/RegionModelChecker.h"

#include <vector>

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"

namespace storm::modelchecker {

template<typename ParametricType>
RegionResult RegionModelChecker<ParametricType>::analyzeRegion(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                                               RegionResultHypothesis const& hypothesis, bool sampleVerticesOfRegion) {
    AnnotatedRegion<ParametricType> annotatedRegion{region};
    monotonicityBackend->initializeMonotonicity(env, annotatedRegion);
    return analyzeRegion(env, annotatedRegion, hypothesis, sampleVerticesOfRegion);
}

template<typename ParametricType>
typename RegionModelChecker<ParametricType>::CoefficientType RegionModelChecker<ParametricType>::getBoundAtInitState(
    Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region, storm::solver::OptimizationDirection const& dirForParameters) {
    AnnotatedRegion<ParametricType> annotatedRegion{region};
    monotonicityBackend->initializeMonotonicity(env, annotatedRegion);
    return getBoundAtInitState(env, annotatedRegion, dirForParameters);
}

template<typename ParametricType>
std::unique_ptr<storm::modelchecker::RegionCheckResult<ParametricType>> RegionModelChecker<ParametricType>::analyzeRegions(
    Environment const& env, std::vector<storm::storage::ParameterRegion<ParametricType>> const& regions, std::vector<RegionResultHypothesis> const& hypotheses,
    bool sampleVerticesOfRegion) {
    STORM_LOG_THROW(regions.size() == hypotheses.size(), storm::exceptions::InvalidArgumentException,
                    "The number of regions and the number of hypotheses do not match.");
    std::vector<std::pair<storm::storage::ParameterRegion<ParametricType>, storm::modelchecker::RegionResult>> result;
    auto hypothesisIt = hypotheses.begin();
    for (auto const& region : regions) {
        storm::modelchecker::RegionResult regionRes = analyzeRegion(env, region, *hypothesisIt, sampleVerticesOfRegion);
        result.emplace_back(region, regionRes);
        ++hypothesisIt;
    }
    return std::make_unique<storm::modelchecker::RegionCheckResult<ParametricType>>(std::move(result));
}

template<typename ParametricType>
RegionSplitEstimateKind RegionModelChecker<ParametricType>::getDefaultRegionSplitEstimateKind(CheckTask<storm::logic::Formula, ParametricType> const&) const {
    return RegionSplitEstimateKind::Distance;
}

template<typename ParametricType>
bool RegionModelChecker<ParametricType>::isRegionSplitEstimateKindSupported(RegionSplitEstimateKind kind,
                                                                            CheckTask<storm::logic::Formula, ParametricType> const&) const {
    return kind == RegionSplitEstimateKind::Distance;
}

template<typename ParametricType>
std::optional<RegionSplitEstimateKind> RegionModelChecker<ParametricType>::getSpecifiedRegionSplitEstimateKind() const {
    return specifiedRegionSplitEstimateKind;
}

template<typename ParametricType>
std::vector<typename RegionModelChecker<ParametricType>::CoefficientType> RegionModelChecker<ParametricType>::obtainRegionSplitEstimates(
    std::set<VariableType> const& relevantParameters) const {
    STORM_LOG_ASSERT(specifiedRegionSplitEstimateKind.has_value(), "Unable to obtain region split estimates because they wre not requested.");
    STORM_LOG_ASSERT(specifiedRegionSplitEstimateKind.value() == RegionSplitEstimateKind::Distance, "requested region split estimate kind not supported");
    STORM_LOG_ASSERT(lastCheckedRegion.has_value(), "Unable to obtain region split estimates because no region was checked.");
    std::vector<CoefficientType> result;
    result.reserve(relevantParameters.size());
    for (auto const& p : relevantParameters) {
        result.push_back(lastCheckedRegion->getDifference(p));
    }
    return result;
}

template<typename ParametricType>
void RegionModelChecker<ParametricType>::specifySplitEstimates(std::optional<RegionSplitEstimateKind> splitEstimates,
                                                               [[maybe_unused]] CheckTask<storm::logic::Formula, ParametricType> const& checkTask) {
    STORM_LOG_ASSERT(!splitEstimates.has_value() || isRegionSplitEstimateKindSupported(splitEstimates.value(), checkTask),
                     "specified region split estimate kind not supported");
    specifiedRegionSplitEstimateKind = splitEstimates;
}

template<typename ParametricType>
void RegionModelChecker<ParametricType>::specifyMonotonicity(std::shared_ptr<MonotonicityBackend<ParametricType>> backend,
                                                             CheckTask<storm::logic::Formula, ParametricType> const& checkTask) {
    if (backend) {
        STORM_LOG_ASSERT(isMonotonicitySupported(*backend, checkTask), "specified monotonicity backend not supported");
        monotonicityBackend = backend;
    } else {
        monotonicityBackend = std::make_shared<MonotonicityBackend<ParametricType>>();
    }
}

template class RegionModelChecker<storm::RationalFunction>;
}  // namespace storm::modelchecker
