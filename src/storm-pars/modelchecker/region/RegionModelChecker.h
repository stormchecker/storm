#pragma once

#include <memory>
#include <optional>

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/modelchecker/region/RegionResultHypothesis.h"
#include "storm-pars/modelchecker/region/RegionSplitEstimateKind.h"
#include "storm-pars/modelchecker/results/RegionCheckResult.h"
#include "storm-pars/storage/ParameterRegion.h"

#include "storm/modelchecker/CheckTask.h"
#include "storm/models/ModelBase.h"

namespace storm {

class Environment;

namespace modelchecker {

// TODO type names are inconsistent and all over the place
template<typename ParametricType>
struct AnnotatedRegion;
template<typename ParametricType>
class MonotonicityBackend;

template<typename ParametricType>
class RegionModelChecker {
   public:
    typedef typename storm::storage::ParameterRegion<ParametricType>::CoefficientType CoefficientType;
    typedef typename storm::storage::ParameterRegion<ParametricType>::VariableType VariableType;
    typedef typename storm::storage::ParameterRegion<ParametricType>::Valuation Valuation;

    RegionModelChecker() = default;
    virtual ~RegionModelChecker() = default;

    virtual bool canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel,
                           CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const = 0;

    virtual void specify(Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel,
                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
                         std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates = std::nullopt,
                         std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend = {}, bool allowModelSimplifications = true,
                         bool graphPreserving = true) = 0;

    /*!
     * Analyzes the given region. Assumes that a property with a threshold was specified.
     * @pre `specify` must be called before.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param hypothesis if not 'unknown', the region checker only tries to show the hypothesis
     * @param sampleVerticesOfRegion enables sampling of the vertices of the region in cases where AllSat/AllViolated could not be shown.
     */
    virtual RegionResult analyzeRegion(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                       RegionResultHypothesis const& hypothesis = RegionResultHypothesis::Unknown, bool sampleVerticesOfRegion = false) = 0;

    /*!
     * Analyzes the given region. Assumes that a property with a threshold was specified.
     * @pre `specify` must be called before.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param hypothesis if not 'unknown', the region checker only tries to show the hypothesis
     * @param sampleVerticesOfRegion enables sampling of the vertices of the region in cases where AllSat/AllViolated could not be shown.
     */
    RegionResult analyzeRegion(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                               RegionResultHypothesis const& hypothesis = RegionResultHypothesis::Unknown, bool sampleVerticesOfRegion = false);

    /*!
     * Analyzes the given regions.
     * @pre `specify` must be called before.
     * @param hypothesis if not 'unknown', we only try to show the hypothesis for each region
     * If supported by this model checker, it is possible to sample the vertices of the regions whenever AllSat/AllViolated could not be shown.
     */
    std::unique_ptr<storm::modelchecker::RegionCheckResult<ParametricType>> analyzeRegions(
        Environment const& env, std::vector<storm::storage::ParameterRegion<ParametricType>> const& regions,
        std::vector<RegionResultHypothesis> const& hypotheses, bool sampleVerticesOfRegion = false);

    /*!
     * Over-approximates the value within the given region. If dirForParameters maximizes, the returned value is an upper bound on the maximum value within the
     * region. If dirForParameters minimizes, the returned value is a lower bound on the minimum value within the region.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether to maximize or minimize the value in the region
     * @return the over-approximated value within the region
     */
    virtual CoefficientType getBoundAtInitState(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                storm::solver::OptimizationDirection const& dirForParameters) = 0;

    /*!
     * Over-approximates the value within the given region. If dirForParameters maximizes, the returned value is an upper bound on the maximum value within the
     * region. If dirForParameters minimizes, the returned value is a lower bound on the minimum value within the region.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether to maximize or minimize the value in the region
     * @return the over-approximated value within the region
     */
    CoefficientType getBoundAtInitState(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                        storm::solver::OptimizationDirection const& dirForParameters);

    /*!
     * Heuristically finds a point within the region and computes the value at the initial state for that point.
     * The heuristic potentially takes annotations from the region such as monotonicity into account. Also data from previous analysis results might be used.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether the heuristic tries to find a point with a high or low value
     * @return a pair of the value at the initial state and the point at which the value was computed
     */
    virtual std::pair<CoefficientType, Valuation> getAndEvaluateGoodPoint(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                          storm::solver::OptimizationDirection const& dirForParameters) = 0;

    /*!
     * @return the default kind of region split estimate that this region model checker generates.
     */
    virtual RegionSplitEstimateKind getDefaultRegionSplitEstimateKind(CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const;

    /*!
     * @return true if this can generate region split estimates of the given kind.
     */
    virtual bool isRegionSplitEstimateKindSupported(RegionSplitEstimateKind kind, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const;

    /*!
     * @return the kind of region split estimation that was selected in the last call to `specify` (if any)
     */
    std::optional<RegionSplitEstimateKind> getSpecifiedRegionSplitEstimateKind() const;

    /*!
     * Returns an estimate of the benefit of splitting the last checked region with respect to each of the given parameters.
     * If a parameter is assigned a high value, we should prefer splitting with respect to this parameter.
     * @pre the last call to `specify` must have set `generateRegionSplitEstimates` to a non-empty value and either `analyzeRegion` or `getBoundAtInitState`
     * must have been called before.
     */
    virtual std::vector<CoefficientType> obtainRegionSplitEstimates(std::set<VariableType> const& relevantParameters) const;

    /*!
     * Returns whether this region model checker can work together with the given monotonicity backend.
     */
    virtual bool isMonotonicitySupported(MonotonicityBackend<ParametricType> const& backend,
                                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const = 0;

   protected:
    virtual void specifySplitEstimates(std::optional<RegionSplitEstimateKind> splitEstimates,
                                       CheckTask<storm::logic::Formula, ParametricType> const& checkTask);
    virtual void specifyMonotonicity(std::shared_ptr<MonotonicityBackend<ParametricType>> backend,
                                     CheckTask<storm::logic::Formula, ParametricType> const& checkTask);

    std::optional<storm::storage::ParameterRegion<ParametricType>> lastCheckedRegion;
    std::optional<RegionSplitEstimateKind> specifiedRegionSplitEstimateKind;
    std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend;
};

}  // namespace modelchecker
}  // namespace storm
