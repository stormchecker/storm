#pragma once

#include "storm-pars/modelchecker/region/RegionModelChecker.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm-pars/utility/parametric.h"

#include "storm/logic/Formulas.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
class SparseInstantiationModelChecker;

/*!
 * Class to approximately check a formula on a parametric model for all parameter valuations within a region
 * It is assumed that all considered valuations are graph-preserving and well defined, i.e.,
 * * all non-const transition probabilities evaluate to some non-zero value
 * * the sum of all outgoing transitions is one
 */
template<typename SparseModelType, typename ConstantType>
class SparseParameterLiftingModelChecker : public RegionModelChecker<typename SparseModelType::ValueType> {
   public:
    using ParametricType = typename SparseModelType::ValueType;
    using CoefficientType = typename RegionModelChecker<ParametricType>::CoefficientType;
    using VariableType = typename RegionModelChecker<ParametricType>::VariableType;
    using Valuation = typename RegionModelChecker<ParametricType>::Valuation;

    SparseParameterLiftingModelChecker();
    virtual ~SparseParameterLiftingModelChecker() = default;

    /*!
     * Analyzes the given region by means of Parameter Lifting. Assumes that a property with a threshold was specified.
     * @pre `specify` must be called before.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param hypothesis if not 'unknown', the region checker only tries to show the hypothesis
     * @param sampleVerticesOfRegion enables sampling of the vertices of the region in cases where AllSat/AllViolated could not be shown.
     */
    virtual RegionResult analyzeRegion(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                       RegionResultHypothesis const& hypothesis = RegionResultHypothesis::Unknown,
                                       bool sampleVerticesOfRegion = false) override;

    /*!
     * Analyzes the 2^#parameters corner points of the given region.
     */
    RegionResult sampleVertices(Environment const& env, storm::storage::ParameterRegion<ParametricType> const& region,
                                RegionResult const& initialResult = RegionResult::Unknown);

    /*!
     * Checks the specified formula on the given region by applying parameter lifting (Parameter choices are lifted to nondeterministic choices)
     * This yields a (sound) approximative model checking result.

     * @param region the region on which parameter lifting is applied
     * @param dirForParameters  The optimization direction for the parameter choices. If this is, e.g., minimize, then the returned result will be a lower bound
     for all results induced by the parameter evaluations inside the region.
     */
    std::unique_ptr<CheckResult> check(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                       storm::solver::OptimizationDirection const& dirForParameters);

    /*!
     * Over-approximates the values within the given region for each state of the considered parametric model. If dirForParameters maximizes, the returned value
     * is an upper bound on the maximum value within the region. If dirForParameters minimizes, the returned value is a lower bound on the minimum value within
     * the region.
     * @pre `specify` must be called before and the model has a single initial state.
     * @param region the region to analyze plus what is already known about this region. The annotations might be updated.
     * @param dirForParameters whether to maximize or minimize the value in the region
     * @return the over-approximated value within the region
     */
    std::unique_ptr<QuantitativeCheckResult<ConstantType>> getBound(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                    storm::solver::OptimizationDirection const& dirForParameters);

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

    SparseModelType const& getConsideredParametricModel() const;
    CheckTask<storm::logic::Formula, ConstantType> const& getCurrentCheckTask() const;

   protected:
    void specifyFormula(Environment const& env, CheckTask<storm::logic::Formula, ParametricType> const& checkTask);

    bool hasUniqueInitialState() const;
    uint64_t getUniqueInitialState() const;

    // Resets all data that correspond to the currently defined property.
    virtual void reset() = 0;

    virtual void specifyBoundedUntilFormula(const CheckTask<storm::logic::BoundedUntilFormula, ConstantType>& checkTask);
    virtual void specifyUntilFormula(Environment const& env, CheckTask<storm::logic::UntilFormula, ConstantType> const& checkTask);
    virtual void specifyReachabilityProbabilityFormula(Environment const& env, CheckTask<storm::logic::EventuallyFormula, ConstantType> const& checkTask);
    virtual void specifyReachabilityRewardFormula(Environment const& env, CheckTask<storm::logic::EventuallyFormula, ConstantType> const& checkTask);
    virtual void specifyCumulativeRewardFormula(const CheckTask<storm::logic::CumulativeRewardFormula, ConstantType>& checkTask);

    virtual storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>& getInstantiationChecker(bool quantitative) = 0;
    virtual storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>& getInstantiationCheckerSAT(bool quantitative);
    virtual storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>& getInstantiationCheckerVIO(bool quantitative);

    virtual std::vector<ConstantType> computeQuantitativeValues(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                storm::solver::OptimizationDirection const& dirForParameters) = 0;

    void updateKnownValueBoundInRegion(AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection dir,
                                       std::vector<ConstantType> const& newValues);

    std::shared_ptr<SparseModelType> parametricModel;
    std::unique_ptr<CheckTask<storm::logic::Formula, ConstantType>> currentCheckTask;

   private:
    // store the current formula. Note that currentCheckTask only stores a reference to the formula.
    std::shared_ptr<storm::logic::Formula const> currentFormula;
};
}  // namespace modelchecker
}  // namespace storm
