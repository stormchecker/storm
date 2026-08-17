#include "storm-pars/modelchecker/region/RegionRefinementChecker.h"

#include <functional>
#include <iterator>
#include <queue>

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/RegionModelChecker.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Bound.h"
#include "storm/logic/ComparisonType.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

template<typename ParametricType>
RegionRefinementChecker<ParametricType>::RegionRefinementChecker(std::unique_ptr<RegionModelChecker<ParametricType>>&& regionChecker) {
    STORM_LOG_ASSERT(regionChecker != nullptr, "The region model checker must not be null.");
    this->regionChecker = std::move(regionChecker);
}

template<typename ParametricType>
bool RegionRefinementChecker<ParametricType>::canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel,
                                                        const CheckTask<storm::logic::Formula, ParametricType>& checkTask) const {
    return regionChecker->canHandle(parametricModel, checkTask);
}

template<typename ParametricType>
void RegionRefinementChecker<ParametricType>::specify(Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel,
                                                      CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
                                                      RegionSplittingStrategy splittingStrategy, std::set<VariableType> const& discreteVariables,
                                                      std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend, bool allowModelSimplifications,
                                                      bool graphPreserving) {
    this->monotonicityBackend = monotonicityBackend ? monotonicityBackend : std::make_shared<MonotonicityBackend<ParametricType>>();
    this->regionSplittingStrategy = std::move(splittingStrategy);
    this->discreteVariables = std::move(discreteVariables);
    this->graphPreserving = graphPreserving;
    if (this->regionSplittingStrategy.heuristic == RegionSplittingStrategy::Heuristic::Default) {
        regionSplittingStrategy.heuristic = RegionSplittingStrategy::Heuristic::EstimateBased;
    }
    // Potentially determine the kind of region split estimate to generate
    if (regionSplittingStrategy.heuristic == RegionSplittingStrategy::Heuristic::EstimateBased) {
        if (regionSplittingStrategy.estimateKind.has_value()) {
            STORM_LOG_THROW(regionChecker->isRegionSplitEstimateKindSupported(regionSplittingStrategy.estimateKind.value(), checkTask),
                            storm::exceptions::NotSupportedException, "The specified region split estimate kind is not supported by the region model checker.");
        } else {
            regionSplittingStrategy.estimateKind = regionChecker->getDefaultRegionSplitEstimateKind(checkTask);
            STORM_LOG_ASSERT(regionChecker->isRegionSplitEstimateKindSupported(regionSplittingStrategy.estimateKind.value(), checkTask),
                             "The region model checker does not support its default region split estimate kind.");
        }
    } else {
        regionSplittingStrategy.estimateKind = std::nullopt;  // do not compute estimates
    }

    regionChecker->specify(env, parametricModel, checkTask, regionSplittingStrategy.estimateKind, monotonicityBackend, allowModelSimplifications,
                           graphPreserving);
}

template<typename T>
class PartitioningProgress {
   public:
    PartitioningProgress(T const totalArea, T const coverageThreshold = storm::utility::zero<T>())
        : totalArea(totalArea),
          coverageThreshold(coverageThreshold),
          fractionOfUndiscoveredArea(storm::utility::one<T>()),
          fractionOfAllSatArea(storm::utility::zero<T>()),
          fractionOfAllViolatedArea(storm::utility::zero<T>()),
          progress("% covered area") {
        progress.setMaxCount(100 - asPercentage(coverageThreshold));
        progress.startNewMeasurement(0u);
    }

    uint64_t getUndiscoveredPercentage() const {
        return asPercentage(fractionOfUndiscoveredArea);
    }

    bool isCoverageThresholdReached() const {
        return fractionOfUndiscoveredArea <= coverageThreshold;
    }

    T addDiscoveredArea(T const& area) {
        auto addedFraction = area / totalArea;
        fractionOfUndiscoveredArea -= addedFraction;
        progress.updateProgress(100 - getUndiscoveredPercentage());
        return addedFraction;
    }

    void addAllSatArea(T const& area) {
        fractionOfAllSatArea += addDiscoveredArea(area);
    }

    void addAllViolatedArea(T const& area) {
        fractionOfAllViolatedArea += addDiscoveredArea(area);
    }

    void addAllIllDefinedArea(T const& area) {
        fractionOfAllIllDefinedArea += addDiscoveredArea(area);
    }

   private:
    static uint64_t asPercentage(T const& value) {
        return storm::utility::convertNumber<uint64_t>(storm::utility::round<T>(value * storm::utility::convertNumber<T, uint64_t>(100u)));
    }

    T const totalArea;
    T const coverageThreshold;
    T fractionOfUndiscoveredArea;
    T fractionOfAllSatArea;
    T fractionOfAllViolatedArea;
    T fractionOfAllIllDefinedArea;
    storm::utility::ProgressMeasurement progress;
};

template<typename ParametricType>
std::unique_ptr<storm::modelchecker::RegionRefinementCheckResult<ParametricType>> RegionRefinementChecker<ParametricType>::performRegionPartitioning(
    Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region, std::optional<ParametricType> coverageThreshold,
    std::optional<uint64_t> depthThreshold, RegionResultHypothesis const& hypothesis, uint64_t monThresh) {
    STORM_LOG_INFO("Applying Region Partitioning on region: " << region.toString(true) << " .");

    auto progress = PartitioningProgress<CoefficientType>(
        region.area(), storm::utility::convertNumber<CoefficientType>(coverageThreshold.value_or(storm::utility::zero<ParametricType>())));

    // Holds the initial region as well as all considered (sub)-regions and their annotations as a tree
    AnnotatedRegion<ParametricType> rootRegion(region);

    // FIFO queue storing the current leafs of the region tree with neither allSat nor allViolated
    // As we currently split regions in the center, a FIFO queue will ensure that regions with a larger area are processed first.
    std::queue<std::reference_wrapper<AnnotatedRegion<ParametricType>>> unprocessedRegions;
    unprocessedRegions.emplace(rootRegion);

    uint64_t numOfAnalyzedRegions{0u};
    bool monotonicityInitialized{false};

    // Region Refinement Loop
    while (!progress.isCoverageThresholdReached() && !unprocessedRegions.empty()) {
        auto& currentRegion = unprocessedRegions.front().get();
        STORM_LOG_TRACE("Analyzing region #" << numOfAnalyzedRegions << " (Refinement depth " << currentRegion.refinementDepth << "; "
                                             << progress.getUndiscoveredPercentage() << "% still unknown; " << unprocessedRegions.size()
                                             << " regions unprocessed).");
        unprocessedRegions.pop();  // can pop already here, since the rootRegion has ownership.
        ++numOfAnalyzedRegions;

        if (!monotonicityInitialized && currentRegion.refinementDepth >= monThresh) {
            monotonicityInitialized = true;
            monotonicityBackend->initializeMonotonicity(env, rootRegion);
            // Propagate monotonicity (unless the currentRegion is the root)
            if (currentRegion.refinementDepth > 0) {
                rootRegion.propagateAnnotationsToSubregions(true);
            }
        }
        if (monotonicityInitialized) {
            monotonicityBackend->updateMonotonicity(env, currentRegion);
        }

        currentRegion.result = regionChecker->analyzeRegion(env, currentRegion, hypothesis);

        if (currentRegion.result == RegionResult::AllSat) {
            progress.addAllSatArea(currentRegion.region.area());
        } else if (currentRegion.result == RegionResult::AllViolated) {
            progress.addAllViolatedArea(currentRegion.region.area());
        } else if (currentRegion.result == RegionResult::AllIllDefined) {
            // ill defined region => not graph-preserving
            STORM_LOG_THROW(!this->graphPreserving, storm::exceptions::NotImplementedException,
                            "The region is not graph-preserving, but the selected region verification engine requires this assumption. Please use "
                            "--assume-graph-preserving false.");
            progress.addAllIllDefinedArea(currentRegion.region.area());
        } else {
            // Split the region as long as the desired refinement depth is not reached.
            if (!depthThreshold || currentRegion.refinementDepth < depthThreshold.value()) {
                monotonicityBackend->updateMonotonicityBeforeSplitting(env, currentRegion);
                auto splittingVariables = getSplittingVariables(currentRegion, Context::Partitioning);
                STORM_LOG_INFO("Splitting on variables" << splittingVariables);
                currentRegion.splitLeafNodeAtCenter(splittingVariables, this->discreteVariables, true);
                for (auto& child : currentRegion.subRegions) {
                    unprocessedRegions.emplace(child);
                }
            }
        }
    }

    // Prepare result
    uint64_t numberOfRegionsKnownThroughMonotonicity{0};
    std::vector<std::pair<storm::storage::ParameterRegion<ParametricType>, RegionResult>> result;
    rootRegion.postOrderTraverseSubRegions([&result, &numberOfRegionsKnownThroughMonotonicity](auto& node) {
        if (node.subRegions.empty()) {
            if (node.resultKnownThroughMonotonicity) {
                ++numberOfRegionsKnownThroughMonotonicity;
            }
            result.emplace_back(node.region, node.result);
        }
    });
    auto const maxDepth = rootRegion.getMaxDepthOfSubRegions();
    STORM_LOG_INFO("Region partitioning terminated after analyzing " << numOfAnalyzedRegions << " regions.\n\t" << numberOfRegionsKnownThroughMonotonicity
                                                                     << " regions known through monotonicity.\n\tMaximum refinement depth: " << maxDepth
                                                                     << ".\n\t" << progress.getUndiscoveredPercentage()
                                                                     << "% of the parameter space are not covered.");

    auto regionCopyForResult = region;
    return std::make_unique<storm::modelchecker::RegionRefinementCheckResult<ParametricType>>(std::move(result), std::move(regionCopyForResult));
}

template<typename ParametricType>
std::pair<typename storm::storage::ParameterRegion<ParametricType>::CoefficientType, typename storm::storage::ParameterRegion<ParametricType>::Valuation>
RegionRefinementChecker<ParametricType>::computeExtremalValueHelper(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                                                    storm::solver::OptimizationDirection const& dir,
                                                                    std::function<bool(CoefficientType, CoefficientType)> acceptGlobalBound,
                                                                    std::function<bool(CoefficientType)> rejectInstance) {
    auto progress = PartitioningProgress<CoefficientType>(region.area());

    // Holds the initial region as well as all considered (sub)-regions and their annotations as a tree
    AnnotatedRegion<ParametricType> rootRegion(region);

    // Priority Queue storing the regions that still need to be processed. Regions with a "good" bound are processed first
    auto cmp = storm::solver::minimize(dir) ? [](AnnotatedRegion<ParametricType> const& lhs,
                                                 AnnotatedRegion<ParametricType> const& rhs) { return *lhs.knownLowerValueBound > *rhs.knownLowerValueBound; }
                                            : [](AnnotatedRegion<ParametricType> const& lhs, AnnotatedRegion<ParametricType> const& rhs) {
                                                  return *lhs.knownUpperValueBound < *rhs.knownUpperValueBound;
                                              };
    std::priority_queue<std::reference_wrapper<AnnotatedRegion<ParametricType>>, std::vector<std::reference_wrapper<AnnotatedRegion<ParametricType>>>,
                        decltype(cmp)>
        unprocessedRegions(cmp);
    unprocessedRegions.push(rootRegion);

    // Initialize Monotonicity
    monotonicityBackend->initializeMonotonicity(env, rootRegion);

    // Initialize result
    auto valueValuation = regionChecker->getAndEvaluateGoodPoint(env, rootRegion, dir);
    auto& value = valueValuation.first;
    if (rejectInstance(value)) {
        return valueValuation;
    }

    // Helper functions to check if a given result is better than the currently known result
    auto isBetterThanValue = [&value, &dir](auto const& newValue) { return storm::solver::minimize(dir) ? newValue < value : newValue > value; };
    // acceptGlobalBound is the opposite of this

    // Region Refinement Loop
    uint64_t numOfAnalyzedRegions{0u};
    while (!unprocessedRegions.empty()) {
        auto& currentRegion = unprocessedRegions.top().get();
        auto currentBound =
            storm::solver::minimize(dir) ? currentRegion.knownLowerValueBound.getOptionalValue() : currentRegion.knownUpperValueBound.getOptionalValue();
        STORM_LOG_TRACE("Analyzing region #" << numOfAnalyzedRegions << " (Refinement depth " << currentRegion.refinementDepth << "; "
                                             << progress.getUndiscoveredPercentage() << "% still unknown; " << unprocessedRegions.size()
                                             << " regions unprocessed). Best known value: " << value << ".");
        unprocessedRegions.pop();  // can pop already here, since the rootRegion has ownership.
        ++numOfAnalyzedRegions;

        monotonicityBackend->updateMonotonicity(env, currentRegion);

        // Compute the bound for this region (unless the known bound is already too weak)
        if (!currentBound || !acceptGlobalBound(value, currentBound.value())) {
            // Improve over-approximation of extremal value (within this region)
            currentBound = regionChecker->getBoundAtInitState(env, currentRegion, dir);
            if (storm::solver::minimize(dir)) {
                currentRegion.knownLowerValueBound &= *currentBound;
            } else {
                currentRegion.knownUpperValueBound &= *currentBound;
            }
        }

        // Process the region if the bound is promising
        if (!acceptGlobalBound(value, currentBound.value())) {
            // Improve (global) under-approximation of extremal value
            // Check whether this region contains a new 'good' value and set this value if that is the case
            auto [currValue, currValuation] = regionChecker->getAndEvaluateGoodPoint(env, currentRegion, dir);
            if (isBetterThanValue(currValue)) {
                valueValuation = {currValue, currValuation};
                if (rejectInstance(currValue)) {
                    STORM_LOG_INFO("Found rejecting instance " << currValuation << " with value " << currValue);
                    return valueValuation;
                }
            }
        }

        // Trigger region-splitting if over- and under-approximation are still too far apart
        if (!acceptGlobalBound(value, currentBound.value())) {
            monotonicityBackend->updateMonotonicityBeforeSplitting(env, currentRegion);
            auto splittingVariables = getSplittingVariables(currentRegion, Context::ExtremalValue);
            STORM_LOG_INFO("Splitting on variables " << splittingVariables);
            currentRegion.splitLeafNodeAtCenter(splittingVariables, this->discreteVariables, true);
            for (auto& child : currentRegion.subRegions) {
                unprocessedRegions.emplace(child);
            }
        } else {
            progress.addDiscoveredArea(currentRegion.region.area());
        }
    }

    std::cout << "Region partitioning for extremal value terminated after analyzing " << numOfAnalyzedRegions << " regions.\n\t"
              << progress.getUndiscoveredPercentage() << "% of the parameter space are not covered.\n";
    return valueValuation;
}

template<typename ParametricType>
std::pair<typename storm::storage::ParameterRegion<ParametricType>::CoefficientType, typename storm::storage::ParameterRegion<ParametricType>::Valuation>
RegionRefinementChecker<ParametricType>::computeExtremalValue(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                                              storm::solver::OptimizationDirection const& dir, ParametricType const& precision,
                                                              bool absolutePrecision, std::optional<storm::logic::Bound> const& boundInvariant) {
    // Handle input precision
    STORM_LOG_THROW(storm::utility::isConstant(precision), storm::exceptions::InvalidArgumentException,
                    "Precision must be a constant value. Got " << precision << " instead.");
    CoefficientType convertedPrecision = storm::utility::convertNumber<CoefficientType>(precision);

    auto acceptGlobalBound = [&](CoefficientType value, CoefficientType newValue) {
        CoefficientType const usedPrecision = convertedPrecision * (absolutePrecision ? storm::utility::one<CoefficientType>() : value);
        return storm::solver::minimize(dir) ? newValue >= value - usedPrecision : newValue <= value + usedPrecision;
    };

    auto rejectInstance = [&](CoefficientType currentValue) { return boundInvariant && !boundInvariant->isSatisfied(currentValue); };

    return computeExtremalValueHelper(env, region, dir, acceptGlobalBound, rejectInstance);
}

template<typename ParametricType>
bool RegionRefinementChecker<ParametricType>::verifyRegion(const storm::Environment& env, const storm::storage::ParameterRegion<ParametricType>& region,
                                                           const storm::logic::Bound& bound) {
    // Use the bound from the formula.
    CoefficientType valueToCheck = storm::utility::convertNumber<CoefficientType>(bound.threshold.evaluateAsRational());
    // We will try to violate the bound.
    storm::solver::OptimizationDirection dir =
        isLowerBound(bound.comparisonType) ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
    // We pass the bound as an invariant; as soon as it is obtained, we can stop the search.
    auto acceptGlobalBound = [&](CoefficientType value, CoefficientType newValue) { return bound.isSatisfied(newValue); };

    auto rejectInstance = [&](CoefficientType currentValue) { return !bound.isSatisfied(currentValue); };

    auto res = computeExtremalValueHelper(env, region, dir, acceptGlobalBound, rejectInstance).first;
    std::cout << "Extremal value: " << res << std::endl;
    return storm::solver::minimize(dir) ? res >= valueToCheck : res <= valueToCheck;
}

template<typename ParametricType>
std::set<typename RegionRefinementChecker<ParametricType>::VariableType> RegionRefinementChecker<ParametricType>::getSplittingVariablesEstimateBased(
    AnnotatedRegion<ParametricType> const& region, Context context) const {
    // If we can split on all variables, do that instead of requesting region split estimates
    if (this->regionSplittingStrategy.maxSplitDimensions >= region.region.getVariables().size()) {
        return region.region.getVariables();
    }

    auto const& estimates = regionChecker->obtainRegionSplitEstimates(region.region.getVariables());
    std::vector<std::pair<VariableType, CoefficientType>> estimatesToSort;
    estimatesToSort.reserve(region.region.getVariables().size());
    STORM_LOG_ASSERT(estimates.size() == region.region.getVariables().size(), "Unexpected number of estimates.");
    auto estimatesIter = estimates.begin();
    for (auto const& param : region.region.getVariables()) {
        estimatesToSort.push_back(std::make_pair(param, *estimatesIter++));
    }

    // Sort and insert largest n=maxSplitDimensions estimates
    std::sort(estimatesToSort.begin(), estimatesToSort.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    std::set<VariableType> splittingVars;
    for (auto const& estimate : estimatesToSort) {
        // Do not split on monotone parameters if finding an extremal value is the goal
        if (context == Context::ExtremalValue && region.monotonicityAnnotation.getGlobalMonotonicityResult() &&
            region.monotonicityAnnotation.getGlobalMonotonicityResult()->isMonotone(estimate.first)) {
            continue;
        }
        // Do not split on parameters that are fixed
        if (region.region.getDifference(estimate.first) == storm::utility::zero<CoefficientType>()) {
            continue;
        }
        splittingVars.emplace(estimate.first);
        if (splittingVars.size() == regionSplittingStrategy.maxSplitDimensions) {
            break;
        }
    }
    return splittingVars;
}

template<typename ParametricType>
std::set<typename RegionRefinementChecker<ParametricType>::VariableType> RegionRefinementChecker<ParametricType>::getSplittingVariablesRoundRobin(
    AnnotatedRegion<ParametricType> const& region, Context context) const {
    // Perform round-robin based on the depth of the region, always split on max split dimensions
    auto const& vars = region.region.getVariables();
    auto varsIter = vars.begin();

    std::advance(varsIter, (region.refinementDepth * this->regionSplittingStrategy.maxSplitDimensions) % vars.size());

    auto loopPoint = varsIter;

    std::set<VariableType> splittingVars;
    do {
        // Do not split on monotone parameters if finding an extremal value is the goal
        if (context == Context::ExtremalValue && region.monotonicityAnnotation.getGlobalMonotonicityResult() &&
            region.monotonicityAnnotation.getGlobalMonotonicityResult()->isMonotone(*varsIter)) {
            continue;
        }
        splittingVars.emplace(*varsIter);
        std::advance(varsIter, 1);
        if (varsIter == vars.end()) {
            varsIter = vars.begin();
        }
    } while (loopPoint != varsIter && splittingVars.size() < this->regionSplittingStrategy.maxSplitDimensions);

    return splittingVars;
}

template<typename ParametricType>
std::set<typename RegionRefinementChecker<ParametricType>::VariableType> RegionRefinementChecker<ParametricType>::getSplittingVariables(
    AnnotatedRegion<ParametricType> const& region, Context context) const {
    switch (regionSplittingStrategy.heuristic) {
        case RegionSplittingStrategy::Heuristic::EstimateBased:
            return getSplittingVariablesEstimateBased(region, context);
        case RegionSplittingStrategy::Heuristic::RoundRobin:
            return getSplittingVariablesRoundRobin(region, context);
        case RegionSplittingStrategy::Heuristic::Default:
        default:
            STORM_LOG_ERROR("Default strategy should have been populated.");
            return {};
    }
}

template class RegionRefinementChecker<storm::RationalFunction>;
}  // namespace storm::modelchecker
