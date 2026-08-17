#pragma once

#include <memory>

#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/modelchecker/region/RegionResultHypothesis.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/modelchecker/results/RegionRefinementCheckResult.h"
#include "storm-pars/storage/ParameterRegion.h"

#include "storm/modelchecker/CheckTask.h"
#include "storm/models/ModelBase.h"

namespace storm {

class Environment;

namespace modelchecker {

template<typename ParametricType>
struct AnnotatedRegion;

template<typename ParametricType>
class RegionModelChecker;

template<typename ParametricType>
class MonotonicityBackend;

template<typename ParametricType>
class RegionRefinementChecker {
   public:
    using CoefficientType = typename storm::storage::ParameterRegion<ParametricType>::CoefficientType;
    using VariableType = typename storm::storage::ParameterRegion<ParametricType>::VariableType;
    using Valuation = typename storm::storage::ParameterRegion<ParametricType>::Valuation;

    RegionRefinementChecker(std::unique_ptr<RegionModelChecker<ParametricType>>&& regionChecker);
    ~RegionRefinementChecker() = default;

    bool canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const;

    void specify(Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel,
                 CheckTask<storm::logic::Formula, ParametricType> const& checkTask, RegionSplittingStrategy splittingStrategy = RegionSplittingStrategy(),
                 std::set<VariableType> const& discreteVariables = {}, std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend = {},
                 bool allowModelSimplifications = true, bool graphPreserving = true);

    /*!
     * Iteratively refines the region until the region analysis yields a conclusive result (AllSat or AllViolated).
     * @param region the considered region
     * @param coverageThreshold if given, the refinement stops as soon as the fraction of the area of the subregions with inconclusive result is less then this
     * threshold
     * @param depthThreshold if given, the refinement stops at the given depth. depth=0 means no refinement.
     * @param hypothesis if not 'unknown', it is only checked whether the hypothesis holds within the given region.
     * @param monThresh if given, determines at which depth to start using monotonicity
     */
    std::unique_ptr<storm::modelchecker::RegionRefinementCheckResult<ParametricType>> performRegionPartitioning(
        Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region, std::optional<ParametricType> coverageThreshold,
        std::optional<uint64_t> depthThreshold = std::nullopt, RegionResultHypothesis const& hypothesis = RegionResultHypothesis::Unknown,
        uint64_t monThresh = 0);

    /*!
     * Finds the extremal value within the given region and with the given precision.
     * The returned value v corresponds to the value at the returned valuation.
     * The actual maximum (minimum) lies in the interval [v, v'] ([v', v])
     * where v' is based on the precision. (With absolute precision, v' = v +/- precision).
     *
     * @param env
     * @param region The region in which to optimize
     * @param dir The direction in which to optimize
     * @param acceptGlobalBound input is a (1) proposed global bound on the value and (2) a new value, output whether we will accept the new value if the global
     * bound holds
     * @param rejectInstance input some value from the parameter space, output whether we will reject because this exists
     * @return
     */
    std::pair<CoefficientType, Valuation> computeExtremalValueHelper(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                                                     storm::solver::OptimizationDirection const& dir,
                                                                     std::function<bool(CoefficientType, CoefficientType)> acceptGlobalBound,
                                                                     std::function<bool(CoefficientType)> rejectInstance);

    /*!
     * Finds the extremal value within the given region and with the given precision.
     * The returned value v corresponds to the value at the returned valuation.
     * The actual maximum (minimum) lies in the interval [v, v'] ([v', v])
     * where v' is based on the precision. (With absolute precision, v' = v +/- precision).
     *
     * @param env
     * @param region The region in which to optimize
     * @param dir The direction in which to optimize
     * @param precision The required precision (unless boundInvariant is set).
     * @param absolutePrecision true iff precision should be measured absolutely
     * @return
     */
    std::pair<CoefficientType, Valuation> computeExtremalValue(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                                               storm::solver::OptimizationDirection const& dir, ParametricType const& precision,
                                                               bool absolutePrecision, std::optional<storm::logic::Bound> const& boundInvariant);

    /*!
     * Checks whether the bound is satisfied on the complete region.
     * @return
     */
    bool verifyRegion(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region, storm::logic::Bound const& bound);

   private:
    enum class Context { Partitioning, ExtremalValue };
    std::set<VariableType> getSplittingVariablesEstimateBased(AnnotatedRegion<ParametricType> const& region, Context context) const;
    std::set<VariableType> getSplittingVariablesRoundRobin(AnnotatedRegion<ParametricType> const& region, Context context) const;

    std::set<VariableType> getSplittingVariables(AnnotatedRegion<ParametricType> const& region, Context context) const;

    std::unique_ptr<RegionModelChecker<ParametricType>> regionChecker;
    std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend;
    RegionSplittingStrategy regionSplittingStrategy;
    std::set<VariableType> discreteVariables;
    bool graphPreserving;
};

}  // namespace modelchecker
}  // namespace storm
