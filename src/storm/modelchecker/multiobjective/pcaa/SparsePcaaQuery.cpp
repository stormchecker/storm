#include "storm/modelchecker/multiobjective/pcaa/SparsePcaaQuery.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/io/export.h"
#include "storm/modelchecker/multiobjective/MultiObjectivePostprocessing.h"
#include "storm/modelchecker/multiobjective/Objective.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/Z3LpSolver.h"
#include "storm/storage/geometry/Hyperrectangle.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm::modelchecker::multiobjective {

template<class SparseModelType, typename GeometryValueType>
SparsePcaaQuery<SparseModelType, GeometryValueType>::SparsePcaaQuery(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>& preprocessorResult)
    : initialStateOfOriginalModel(preprocessorResult.originalModel.getInitialStates().getNextSetIndex(0)), objectives(preprocessorResult.objectives) {
    STORM_LOG_THROW(preprocessorResult.originalModel.getInitialStates().hasUniqueSetBit(), storm::exceptions::NotSupportedException,
                    "The input model does not have a unique initial state.");
    this->weightVectorChecker = createWeightVectorChecker(preprocessorResult);
}

template<class SparseModelType, typename GeometryValueType>
std::unique_ptr<CheckResult> SparsePcaaQuery<SparseModelType, GeometryValueType>::check(Environment const& env, bool produceScheduler) {
    // Following Algorithm 3.1 of https://doi.org/10.18154/RWTH-2023-09669

    // Ensure that we can handle the input
    STORM_LOG_THROW(env.modelchecker().multi().getPrecisionType() == MultiObjectiveModelCheckerEnvironment::PrecisionType::Absolute,
                    storm::exceptions::IllegalArgumentException, "Unhandled multiobjective precision type.");

    // Auxiliary helper functions for better readability
    auto abortIterations = [&env](uint64_t numRefinementSteps) {
        if (env.modelchecker().multi().isMaxStepsSet() && numRefinementSteps >= env.modelchecker().multi().getMaxSteps()) {
            STORM_LOG_WARN("Aborting multi-objective computation as the maximum number of refinement steps (" << env.modelchecker().multi().getMaxSteps()
                                                                                                              << ") has been reached.");
            return true;
        } else if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting multi-objective computation after " << numRefinementSteps << " refinement steps as termination has been requested.");
            return true;
        }
        return false;
    };
    auto isMinimizingObjective = [this](uint64_t objIndex) { return storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()); };

    // Data maintained through the iterations
    // The results in each iteration of the algorithm (including achievable points)
    std::vector<RefinementStep> refinementSteps;
    // Over-approximation of the set of achievable points
    PolytopePtr overApproximation(Polytope::createUniversalPolytope());

    // Start iterative refinement
    while (!abortIterations(refinementSteps.size())) {
        auto answerOrWeights = tryAnswerOrNextWeights(env, refinementSteps, overApproximation, produceScheduler);
        if (answerOrWeights.index() == 0) {
            if (env.modelchecker().multi().isExportPlotSet()) {
                exportPlotOfCurrentApproximation(env, refinementSteps, overApproximation);
            }
            if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
                STORM_PRINT_AND_LOG("Multi-objective Pareto Curve Approximation algorithm terminated after " << refinementSteps.size()
                                                                                                             << " refinement steps.\n");
            }
            return std::move(std::get<0>(answerOrWeights));
        }
        auto [weightVector, epsilonWso] = std::get<1>(answerOrWeights);
        // Normalize the weight vector to make sure that its magnitude does not influence the accuracy of the weighted sum optimization
        GeometryValueType normalizationFactor =
            storm::utility::one<GeometryValueType>() / storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector));
        storm::utility::vector::scaleVectorInPlace(weightVector, normalizationFactor);
        STORM_LOG_INFO("Iteration #" << refinementSteps.size() << ": Processing new WSO instance with weight vector "
                                     << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(weightVector))
                                     << " and precision " << storm::utility::convertNumber<double>(epsilonWso) << ".");

        // Solve WSO instance
        this->weightVectorChecker->setWeightedPrecision(storm::utility::convertNumber<ModelValueType>(epsilonWso));
        weightVectorChecker->check(env, storm::utility::vector::convertNumericVector<ModelValueType>(weightVector));
        GeometryValueType optimalWeightedSum = storm::utility::convertNumber<GeometryValueType>(weightVectorChecker->getOptimalWeightedSum());
        // Due to numerical issues, it might be that the found optimal weighted sum is smaller than the actual weighted sum of one of the achievable points.
        // To avoid that our over-approximation does not contain all achievable points, we correct this here.
        for (auto const& step : refinementSteps) {
            optimalWeightedSum = std::max(optimalWeightedSum, storm::utility::vector::dotProduct(weightVector, step.achievablePoint));
        }
        if (GeometryValueType const diff = optimalWeightedSum - storm::utility::convertNumber<GeometryValueType>(weightVectorChecker->getOptimalWeightedSum());
            diff > epsilonWso / 10) {
            STORM_LOG_WARN("Numerical issues: The overapproximation would not contain the underapproximation. Hence, a halfspace is shifted by "
                           << (storm::utility::convertNumber<double>(diff)) << ".");
        }

        // Store result of iteration
        refinementSteps.push_back(
            RefinementStep{.weightVector{std::move(weightVector)},
                           .achievablePoint{storm::utility::vector::convertNumericVector<GeometryValueType>(weightVectorChecker->getAchievablePoint())},
                           .optimalWeightedSum{optimalWeightedSum},
                           .scheduler{}});
        auto& currentStep = refinementSteps.back();
        STORM_LOG_INFO(
            "WSO found point " << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(currentStep.achievablePoint)));
        // For the minimizing objectives, we need to scale the corresponding entries with -1 as we want to consider the downward closure
        for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
            if (isMinimizingObjective(objIndex)) {
                currentStep.achievablePoint[objIndex] *= -storm::utility::one<GeometryValueType>();
            }
        }
        if (produceScheduler) {
            currentStep.scheduler = weightVectorChecker->computeScheduler();
        }
        overApproximation =
            overApproximation->intersection(storm::storage::geometry::Halfspace<GeometryValueType>(currentStep.weightVector, currentStep.optimalWeightedSum));
    }
    // Reaching this means that we aborted the iterations
    // Return a best-effort solution
    std::vector<std::vector<ModelValueType>> achievablePoints;
    achievablePoints.reserve(refinementSteps.size());
    for (auto const& step : refinementSteps) {
        achievablePoints.push_back(
            storm::utility::vector::convertNumericVector<ModelValueType>(transformObjectiveValuesToOriginal(objectives, step.achievablePoint)));
    }
    return std::unique_ptr<CheckResult>(new ExplicitParetoCurveCheckResult<ModelValueType>(initialStateOfOriginalModel, std::move(achievablePoints)));
}

template<class SparseModelType, typename GeometryValueType>
typename SparsePcaaQuery<SparseModelType, GeometryValueType>::AnswerOrWeights SparsePcaaQuery<SparseModelType, GeometryValueType>::tryAnswerOrNextWeights(
    Environment const& env, std::vector<RefinementStep> const& refinementSteps, PolytopePtr overApproximation, bool produceScheduler) {
    if (refinementSteps.size() < objectives.size()) {
        // At least optimize each objective once
        WeightVector weightVector(objectives.size(), storm::utility::zero<GeometryValueType>());
        weightVector[refinementSteps.size()] = storm::utility::one<GeometryValueType>();

        return WeightedSumOptimizationInput{
            .weightVector{std::move(weightVector)},
            .epsilonWso{getEpsilonWso(env)},
        };
    }
    storm::storage::BitVector objectivesWithThreshold(objectives.size(), false);
    std::vector<GeometryValueType> thresholds(objectives.size(), storm::utility::zero<GeometryValueType>());
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        auto const& formula = *objectives[objIndex].formula;
        if (formula.hasBound()) {
            objectivesWithThreshold.set(objIndex);
            thresholds[objIndex] = formula.template getThresholdAs<GeometryValueType>();
            if (storm::solver::minimize(formula.getOptimalityType())) {
                // Values for minimizing objectives will be negated in order to convert them to maximizing objectives.
                thresholds[objIndex] *= -storm::utility::one<GeometryValueType>();
            }
            STORM_LOG_WARN_COND(
                !storm::logic::isStrict(formula.getBound().comparisonType),
                "Strict bound in objective " << objectives[objIndex].originalFormula << " is not supported and will be treated as non-strict bound.");
        }
    }
    if (objectivesWithThreshold.empty() && objectives.size() > 1) {
        return tryAnswerOrNextWeightsPareto(env, refinementSteps, overApproximation, produceScheduler);
    } else {
        uint64_t const numObjectivesWithoutBound = objectives.size() - objectivesWithThreshold.getNumberOfSetBits();
        std::optional<uint64_t> optObjIndex;
        if (numObjectivesWithoutBound == 1) {
            optObjIndex = objectivesWithThreshold.getNextUnsetIndex(0);
        } else {
            STORM_LOG_THROW(numObjectivesWithoutBound == 0, storm::exceptions::NotSupportedException,
                            "The type of query is not supported: There are multiple objectives with and without a value bound.");
        }
        return tryAnswerOrNextWeightsAchievability(env, optObjIndex, thresholds, refinementSteps, overApproximation, produceScheduler);
    }
}

template<typename GeometryValueType>
auto findSeparatingHalfspace(auto const& refinementSteps, std::vector<GeometryValueType> const& point) {
    // Build the LP from Figure 3.9 of https://doi.org/10.18154/RWTH-2023-09669
    uint64_t const dim = point.size();
    STORM_LOG_ASSERT(dim > 0, "Expected at least one dimension for separating halfspace computation.");
    STORM_LOG_ASSERT(!refinementSteps.empty(), "Expected at least one refinement step for separating halfspace computation.");
    auto const zero = storm::utility::zero<GeometryValueType>();
    auto const one = storm::utility::one<GeometryValueType>();

    storm::solver::Z3LpSolver<GeometryValueType> solver(storm::solver::OptimizationDirection::Maximize);
    std::vector<storm::expressions::Expression> weightVariableExpressions;
    weightVariableExpressions.reserve(dim);
    for (uint64_t i = 0; i < dim; ++i) {
        weightVariableExpressions.push_back(solver.addBoundedContinuousVariable("w" + std::to_string(i), zero, one));
    }
    solver.addConstraint("", storm::expressions::sum(weightVariableExpressions) <= solver.getManager().rational(one));
    auto distVar = solver.addUnboundedContinuousVariable("d", one);
    for (auto const& step : refinementSteps) {
        std::vector<storm::expressions::Expression> sum;
        sum.reserve(dim);
        for (uint64_t i = 0; i < dim; ++i) {
            sum.push_back(solver.getManager().rational(point[i] - step.achievablePoint[i]) * weightVariableExpressions[i]);
        }
        solver.addConstraint("", distVar <= storm::expressions::sum(sum));
    }
    solver.update();
    solver.optimize();
    std::optional<storm::storage::geometry::Halfspace<GeometryValueType>> result;
    if (solver.isOptimal()) {
        std::vector<GeometryValueType> normalVector;
        for (auto const& w_i : weightVariableExpressions) {
            normalVector.push_back(solver.getContinuousValue(w_i.getBaseExpression().asVariableExpression().getVariable()));
        }
        GeometryValueType offset = storm::utility::vector::dotProduct(normalVector, point) - solver.getContinuousValue(distVar);
        result.emplace(std::move(normalVector), std::move(offset));
    } else {
        STORM_LOG_THROW(solver.isInfeasible(), storm::exceptions::UnexpectedException, "Unexpected result of LP solver in separating halfspace computation.");
    }
    return result;
}

template<class SparseModelType, typename GeometryValueType>
typename SparsePcaaQuery<SparseModelType, GeometryValueType>::AnswerOrWeights
SparsePcaaQuery<SparseModelType, GeometryValueType>::tryAnswerOrNextWeightsAchievability(Environment const& env, std::optional<uint64_t> const optObjIndex,
                                                                                         std::vector<GeometryValueType> const& thresholds,
                                                                                         std::vector<RefinementStep> const& refinementSteps,
                                                                                         PolytopePtr overApproximation, bool produceScheduler) {
    // First use the overapproximation to either
    // (1) decide that the thresholds are not achievable or
    // (2) obtain a reference point that is in the over-approximation, respects the thresholds, and is epsilon-optimal for the objective without threshold (if
    // any)
    Point referencePoint;
    if (!optObjIndex.has_value()) {
        if (!overApproximation->contains(thresholds)) {
            // The thresholds are not achievable
            return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ModelValueType>(initialStateOfOriginalModel, false));
        }
        referencePoint = thresholds;
    } else {
        // Get the best value we can hope to achieve
        std::vector<Halfspace> thresholdsHalfspaces;
        for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
            if (objIndex == optObjIndex.value()) {
                continue;
            }
            thresholdsHalfspaces.push_back(Halfspace(WeightVector(objectives.size(), storm::utility::zero<GeometryValueType>()), -thresholds[objIndex]));
            thresholdsHalfspaces.back().normalVector()[objIndex] = -storm::utility::one<GeometryValueType>();
        }
        auto thresholdPolytope = Polytope::create(thresholdsHalfspaces);
        auto intersection = overApproximation->intersection(thresholdPolytope);
        WeightVector optDirVector(objectives.size(), storm::utility::zero<GeometryValueType>());
        optDirVector[optObjIndex.value()] = storm::utility::one<GeometryValueType>();
        auto optRes = overApproximation->intersection(thresholdPolytope)->optimize(optDirVector);
        if (!optRes.second) {
            // The thresholds are not achievable
            return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ModelValueType>(initialStateOfOriginalModel, false));
        }
        referencePoint = thresholds;
        referencePoint[optObjIndex.value()] =
            optRes.first[optObjIndex.value()] - storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision());
        // The following assertion holds because optRes.first is in the over-approximation and satisfies all thresholds and the over-approximation is
        // downward closed
        STORM_LOG_ASSERT(overApproximation->contains(referencePoint), "Expected reference point to be contained in the over-approximation");
    }

    // Second, find a separating halfspace between the under-approximation and the reference point with maximal L1 distance (not Euclidean!) to the latter
    auto separatingHalfspace = findSeparatingHalfspace(refinementSteps, referencePoint);
    bool const referencePointInUnderApproximation = !separatingHalfspace.has_value() || separatingHalfspace->contains(referencePoint);
    if (referencePointInUnderApproximation) {
        // The reference point is achievable. We can assemble a result
        STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotSupportedException,
                        "Producing schedulers is currently not supported for (numerical) achievability queries.");
        // TODO: to get a scheduler, we need to find a convex combination of the achievable points that yields the reference point (using LP)
        if (optObjIndex.has_value()) {
            // Return the middle-value of the result interval [ referencePoint[optObjIndex], referencePoint[optObjIndex] + multiPrecisoin ]
            GeometryValueType result =
                referencePoint[optObjIndex.value()] + (storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision()) /
                                                       storm::utility::convertNumber<GeometryValueType, uint64_t>(2));
            auto resultForOriginalModel =
                storm::utility::convertNumber<ModelValueType>(transformObjectiveValueToOriginal(objectives[optObjIndex.value()], result));

            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ModelValueType>(initialStateOfOriginalModel, resultForOriginalModel));
        } else {
            return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ModelValueType>(initialStateOfOriginalModel, true));
        }
    }

    // We found a separating halfspace that we can use to refine the approximation
    GeometryValueType eps_wso = getEpsilonWso(env);
    // Check if there is a need to increase the weighted sum optimization precision
    if (separatingHalfspace->distance(referencePoint) < eps_wso) {
        // The reference point is close to the under-approximation. We also check if the boundary of the over-approximation is close
        auto optResPair = overApproximation->optimize(separatingHalfspace->normalVector());
        STORM_LOG_ASSERT(optResPair.second, "Expected optimization to be successful as the over-approximation is non-empty.");
        eps_wso = getEpsilonWso(env, separatingHalfspace->distance(optResPair.first));
    }
    return WeightedSumOptimizationInput{
        .weightVector{separatingHalfspace->normalVector()},
        .epsilonWso{eps_wso},
    };
}

template<class SparseModelType, typename GeometryValueType>
typename SparsePcaaQuery<SparseModelType, GeometryValueType>::AnswerOrWeights SparsePcaaQuery<SparseModelType, GeometryValueType>::tryAnswerOrNextWeightsPareto(
    Environment const& env, std::vector<RefinementStep> const& refinementSteps, PolytopePtr overApproximation, bool produceScheduler) {
    // First get the halfspaces whose intersection underapproximates the set of achievable points
    std::vector<Point> achievablePoints;
    achievablePoints.reserve(refinementSteps.size());
    for (auto const& step : refinementSteps) {
        achievablePoints.push_back(step.achievablePoint);
    }
    PolytopePtr underApproximation = Polytope::createDownwardClosure(achievablePoints);
    auto achievableHalfspaces = underApproximation->getHalfspaces();
    // Now check whether the over-approximation contains a point that is not close enough to the under-approximation
    GeometryValueType delta = storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision()) /
                              storm::utility::convertNumber<GeometryValueType>(std::sqrt(objectives.size()));
    for (auto const& halfspace : achievableHalfspaces) {
        GeometryValueType const sumOfWeights =
            std::accumulate(halfspace.normalVector().begin(), halfspace.normalVector().end(), storm::utility::zero<GeometryValueType>());
        auto invertedShiftedHalfspace = halfspace.invert();
        invertedShiftedHalfspace.offset() -= delta * sumOfWeights;
        auto intersection = overApproximation->intersection(invertedShiftedHalfspace);
        if (!intersection->isEmpty()) {
            return WeightedSumOptimizationInput{
                .weightVector{halfspace.normalVector()},
                .epsilonWso{getEpsilonWso(env)},
            };
        }
    }
    // If we reach this point, the over-approximation is close enough to the under-approximation
    // obtain the data for the checkresult
    // We take the paretoOptimalPoints as the vertices of the underApproximation.
    // This is to filter out points found in a refinement step that are dominated by another point.
    std::vector<std::vector<ModelValueType>> paretoOptimalPoints;
    std::vector<storm::storage::Scheduler<ModelValueType>> paretoOptimalSchedulers;
    std::vector<Point> vertices = underApproximation->getVertices();
    paretoOptimalPoints.reserve(vertices.size());
    for (auto const& vertex : vertices) {
        paretoOptimalPoints.push_back(
            storm::utility::vector::convertNumericVector<ModelValueType>(transformObjectiveValuesToOriginal(this->objectives, vertex)));
        if (produceScheduler) {
            // Find the refinement step in which we found the vertex
            // This is guaranteed to work as long as GeometryValueType is exact, i.e.,
            // there as long as there are  no rounding errors when converting from set of points into a (H-)polytope and then back to a vertex set.
            static_assert(storm::NumberTraits<GeometryValueType>::IsExact);
            auto stepIt = std::find_if(refinementSteps.begin(), refinementSteps.end(), [&vertex](auto const& step) { return step.achievablePoint == vertex; });
            STORM_LOG_ASSERT(stepIt != refinementSteps.end(),
                             "Scheduler for point " << storm::utility::vector::toString(paretoOptimalPoints.back()) << " not found.");
            STORM_LOG_ASSERT(stepIt->scheduler.has_value(),
                             "Scheduler for point " << storm::utility::vector::toString(paretoOptimalPoints.back()) << " not generated.");
            paretoOptimalSchedulers.push_back(std::move(stepIt->scheduler.value()));
        }
    }
    return std::unique_ptr<CheckResult>(new ExplicitParetoCurveCheckResult<ModelValueType>(
        initialStateOfOriginalModel, std::move(paretoOptimalPoints), std::move(paretoOptimalSchedulers),
        transformObjectivePolytopeToOriginal(this->objectives, underApproximation)->template convertNumberRepresentation<ModelValueType>(),
        transformObjectivePolytopeToOriginal(this->objectives, overApproximation)->template convertNumberRepresentation<ModelValueType>()));
}

template<typename SparseModelType, typename GeometryValueType>
GeometryValueType SparsePcaaQuery<SparseModelType, GeometryValueType>::getEpsilonWso(Environment const& env, std::optional<GeometryValueType> approxDistance) {
    // Determine heuristic parameter gamma  for approximation tradeoff. We should have 0 < gamma < 1, where small values mean that weighted sum optimization
    // needs to be done with high accuracy.
    GeometryValueType gamma;
    if (env.modelchecker().multi().isApproximationTradeoffSet()) {
        // A value was set explicitly, so we use that.
        gamma = storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getApproximationTradeoff());
    } else {
        // No value was set explicitly. We pick one heuristically
        if (env.solver().isForceExact()) {
            gamma = storm::utility::zero<GeometryValueType>();  // In exact mode, we don't expect any inaccuracies in the WSO solver
        } else if (env.solver().isForceSoundness() || weightVectorChecker->smallPrecisionsAreChallenging()) {
            // in sound mode and/or when WSO calls are challenging, we pick a middle-ground value
            gamma = storm::utility::convertNumber<GeometryValueType>(0.5);
        } else {
            // In unsound mode with non-challenging WSO calls, we don't want too inaccurate precisions (e.g. standard value iteration with large epsilon becomes
            // very unreliable). Hence, we pick a rather small value.
            gamma = storm::utility::convertNumber<GeometryValueType>(1.0 / 64.0);
        }
    }

    // Get the precision for multiobjective model checking. Further decrease it if the approximation is close.
    GeometryValueType eps_multi = storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision());
    if (approxDistance.has_value()) {
        eps_multi = std::min<GeometryValueType>(eps_multi, approxDistance.value());
    }

    // We divide by sqrt(objectives.size()) to ensure that even for values of gamma close to 1, we can still achieve enough precision
    // See Example 3.5 in https://doi.org/10.18154/RWTH-2023-09669 for an example why this is needed.
    return gamma * eps_multi / storm::utility::convertNumber<GeometryValueType>(std::sqrt(objectives.size()));
}

template<typename SparseModelType, typename GeometryValueType>
void SparsePcaaQuery<SparseModelType, GeometryValueType>::exportPlotOfCurrentApproximation(Environment const& env,
                                                                                           std::vector<RefinementStep> const& refinementSteps,
                                                                                           PolytopePtr overApproximation) const {
    STORM_LOG_ERROR_COND(objectives.size() == 2, "Exporting plot requested but this is only implemented for the two-dimensional case.");

    // Get achievable points as well as a hyperrectangle that is used to guarantee that the resulting polytopes are bounded.
    storm::storage::geometry::Hyperrectangle<GeometryValueType> boundaries(
        std::vector<GeometryValueType>(objectives.size(), storm::utility::zero<GeometryValueType>()),
        std::vector<GeometryValueType>(objectives.size(), storm::utility::zero<GeometryValueType>()));
    std::vector<std::vector<GeometryValueType>> achievablePoints;
    achievablePoints.reserve(refinementSteps.size());
    for (auto const& step : refinementSteps) {
        achievablePoints.push_back(transformObjectiveValuesToOriginal(this->objectives, step.achievablePoint));
        boundaries.enlarge(achievablePoints.back());
    }

    PolytopePtr underApproximation = Polytope::createDownwardClosure(achievablePoints);
    auto transformedUnderApprox = transformObjectivePolytopeToOriginal(this->objectives, underApproximation);
    auto transformedOverApprox = transformObjectivePolytopeToOriginal(this->objectives, overApproximation);

    auto underApproxVertices = transformedUnderApprox->getVertices();
    for (auto const& v : underApproxVertices) {
        boundaries.enlarge(v);
    }
    auto overApproxVertices = transformedOverApprox->getVertices();
    for (auto const& v : overApproxVertices) {
        boundaries.enlarge(v);
    }

    // Further enlarge the boundaries a little
    storm::utility::vector::scaleVectorInPlace(boundaries.lowerBounds(), GeometryValueType(15) / GeometryValueType(10));
    storm::utility::vector::scaleVectorInPlace(boundaries.upperBounds(), GeometryValueType(15) / GeometryValueType(10));

    auto boundariesAsPolytope = boundaries.asPolytope();
    std::vector<std::string> columnHeaders = {"x", "y"};

    std::vector<std::vector<double>> pointsForPlotting;
    if (env.modelchecker().multi().getPlotPathUnderApproximation()) {
        underApproxVertices = transformedUnderApprox->intersection(boundariesAsPolytope)->getVerticesInClockwiseOrder();
        pointsForPlotting.reserve(underApproxVertices.size());
        for (auto const& v : underApproxVertices) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathUnderApproximation().get(), pointsForPlotting, columnHeaders);
    }

    if (env.modelchecker().multi().getPlotPathOverApproximation()) {
        pointsForPlotting.clear();
        overApproxVertices = transformedOverApprox->intersection(boundariesAsPolytope)->getVerticesInClockwiseOrder();
        pointsForPlotting.reserve(overApproxVertices.size());
        for (auto const& v : overApproxVertices) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathOverApproximation().get(), pointsForPlotting, columnHeaders);
    }

    if (env.modelchecker().multi().getPlotPathParetoPoints()) {
        pointsForPlotting.clear();
        pointsForPlotting.reserve(achievablePoints.size());
        for (auto const& v : achievablePoints) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathParetoPoints().get(), pointsForPlotting, columnHeaders);
    }
}

template class SparsePcaaQuery<storm::models::sparse::Mdp<double>, storm::RationalNumber>;
template class SparsePcaaQuery<storm::models::sparse::MarkovAutomaton<double>, storm::RationalNumber>;

template class SparsePcaaQuery<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber>;
template class SparsePcaaQuery<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>, storm::RationalNumber>;
}  // namespace storm::modelchecker::multiobjective
