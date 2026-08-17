#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/storage/geometry/Polytope.h"

namespace storm {

class Environment;

namespace modelchecker::multiobjective {

/*
 * This class represents a query for the Pareto curve approximation algorithm (Pcaa).
 * It implements the necessary computations for the different query types.
 * @See Section 3.4 of https://doi.org/10.18154/RWTH-2023-09669
 */
template<class SparseModelType, typename GeometryValueType>
class SparsePcaaQuery {
   public:
    using Point = std::vector<GeometryValueType>;
    using WeightVector = std::vector<GeometryValueType>;
    using Polytope = storm::storage::geometry::Polytope<GeometryValueType>;
    using Halfspace = storm::storage::geometry::Halfspace<GeometryValueType>;
    using PolytopePtr = std::shared_ptr<Polytope>;
    using PreprocessorResult = preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>;
    using ModelValueType = typename SparseModelType::ValueType;

    /*!
     * Creates a new query for the Pareto curve approximation algorithm (Pcaa)
     * @param preprocessorResult the result from preprocessing
     */
    SparsePcaaQuery(PreprocessorResult& preprocessorResult);

    /*!
     * Invokes the computation and retrieves the result
     */
    std::unique_ptr<CheckResult> check(Environment const& env, bool produceScheduler);

   private:
    /*!
     * Represents the information obtained in a single iteration of the algorithm
     * We have achievablePoint * weightVector <= weightedSum
     * The provided scheduler achieves the achievablePoint (if it is set)
     * This assumes that minimizing objectives are implicitly negated. We have (using v_i as the actual value for objective i induced by the scheduler):
     * (*) if objective i is Maximizing: achievablePoint[i] <= v_i
     * (*) if objective i is Minimizing: achievablePoint[i] <= -v_i (or equivalently, -achievablePoint[i] >= v_i)
     */
    struct RefinementStep {
        WeightVector weightVector;
        Point achievablePoint;
        GeometryValueType optimalWeightedSum;
        std::optional<storm::storage::Scheduler<ModelValueType>> scheduler;
    };

    struct WeightedSumOptimizationInput {
        WeightVector weightVector;
        GeometryValueType epsilonWso;
    };
    using AnswerOrWeights = std::variant<std::unique_ptr<CheckResult>, WeightedSumOptimizationInput>;
    AnswerOrWeights tryAnswerOrNextWeights(Environment const& env, std::vector<RefinementStep> const& refinementSteps, PolytopePtr overApproximation,
                                           bool produceScheduler);
    AnswerOrWeights tryAnswerOrNextWeightsAchievability(Environment const& env, std::optional<uint64_t> const optObjIndex,
                                                        std::vector<GeometryValueType> const& thresholds, std::vector<RefinementStep> const& refinementSteps,
                                                        PolytopePtr overApproximation, bool produceScheduler);
    AnswerOrWeights tryAnswerOrNextWeightsPareto(Environment const& env, std::vector<RefinementStep> const& refinementSteps, PolytopePtr overApproximation,
                                                 bool produceScheduler);

    /*!
     * @param approxDistance the current distance between over- and under approximation. Can be used to enforce that epsilonWso becomes smaller as the
     * approximations get closer.
     * @return the desired precision for the weighted sum optimization
     */
    GeometryValueType getEpsilonWso(Environment const& env, std::optional<GeometryValueType> approxDistance = std::nullopt);

    /*!
     * Exports the current approximations and the currently processed points into respective .csv files located at the given directory.
     * The polytopes are represented as the set of vertices.
     * Note that the approximations will be intersected with a  (sufficiently large) hyperrectangle in order to ensure that the polytopes are bounded
     * This only works for 2 dimensional queries.
     */
    void exportPlotOfCurrentApproximation(Environment const& env, std::vector<RefinementStep> const& refinementSteps, PolytopePtr overApproximation) const;

    uint64_t const initialStateOfOriginalModel;  // needed to prepare the CheckResult.
    std::vector<Objective<ModelValueType>> objectives;

    // The corresponding weight vector checker
    std::unique_ptr<PcaaWeightVectorChecker<SparseModelType>> weightVectorChecker;
};

}  // namespace modelchecker::multiobjective
}  // namespace storm
