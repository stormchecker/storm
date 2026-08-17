#pragma once

#include "storm-pars/modelchecker/region/RegionModelChecker.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/SparseMdpParameterLiftingModelChecker.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
class ValidatingSparseParameterLiftingModelChecker : public RegionModelChecker<typename SparseModelType::ValueType> {
    static_assert(storm::NumberTraits<PreciseType>::IsExact, "Specified type for exact computations is not exact.");

    using ParametricType = typename SparseModelType::ValueType;
    using CoefficientType = typename RegionModelChecker<ParametricType>::CoefficientType;
    using VariableType = typename RegionModelChecker<ParametricType>::VariableType;
    using Valuation = typename RegionModelChecker<ParametricType>::Valuation;

   public:
    ValidatingSparseParameterLiftingModelChecker();
    virtual ~ValidatingSparseParameterLiftingModelChecker();

    virtual bool canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel,
                           CheckTask<storm::logic::Formula, typename SparseModelType::ValueType> const& checkTask) const override;

    virtual void specify(Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel,
                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
                         std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates = std::nullopt,
                         std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend = {}, bool allowModelSimplifications = true,
                         bool graphPreserving = true) override;

    /*!
     * Analyzes the given region. Assumes that a property with a threshold was specified.
     * We first apply unsound solution methods (e.g. standard value iteration with doubles) and then validate the obtained result
     * by means of exact and sound methods.
     * @pre `specify` must be called before.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param hypothesis if not 'unknown', the region checker only tries to show the hypothesis
     * @param sampleVerticesOfRegion enables sampling of the vertices of the region in cases where AllSat/AllViolated could not be shown.
     */
    virtual RegionResult analyzeRegion(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                       RegionResultHypothesis const& hypothesis = RegionResultHypothesis::Unknown,
                                       bool sampleVerticesOfRegion = false) override;

    /*!
     * Over-approximates the value within the given region. If dirForParameters maximizes, the returned value is an upper bound on the maximum value within the
     * region. If dirForParameters minimizes, the returned value is a lower bound on the minimum value within the region.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether to maximize or minimize the value in the region
     * @return the over-approximated value within the region
     */
    virtual CoefficientType getBoundAtInitState(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                storm::solver::OptimizationDirection const& dirForParameters) override;

    /*!
     * Heuristically finds a point within the region and computes the value at the initial state for that point.
     * The heuristic potentially takes annotations from the region such as monotonicity into account. Also data from previous analysis results might be used.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether the heuristic tries to find a point with a high or low value
     * @return a pair of the value at the initial state and the point at which the value was computed
     */
    virtual std::pair<CoefficientType, Valuation> getAndEvaluateGoodPoint(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                          storm::solver::OptimizationDirection const& dirForParameters) override;

    /*!
     * Returns whether this region model checker can work together with the given monotonicity backend.
     */
    virtual bool isMonotonicitySupported(MonotonicityBackend<ParametricType> const& backend,
                                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const override;

   private:
    static constexpr bool IsMDP = std::is_same_v<SparseModelType, storm::models::sparse::Mdp<ParametricType>>;
    static constexpr bool IsDTMC = std::is_same_v<SparseModelType, storm::models::sparse::Dtmc<ParametricType>>;
    static_assert(IsMDP || IsDTMC, "Model type is neither MDP nor DTMC.");

    template<typename T>
    using UnderlyingCheckerType = typename std::conditional_t<IsMDP, SparseMdpParameterLiftingModelChecker<SparseModelType, T>,
                                                              SparseDtmcParameterLiftingModelChecker<SparseModelType, T>>;

    UnderlyingCheckerType<ImpreciseType> impreciseChecker;
    UnderlyingCheckerType<PreciseType> preciseChecker;

    void applyHintsToPreciseChecker();

    // Information for statistics
    uint_fast64_t numOfWrongRegions;
};
}  // namespace modelchecker
}  // namespace storm
