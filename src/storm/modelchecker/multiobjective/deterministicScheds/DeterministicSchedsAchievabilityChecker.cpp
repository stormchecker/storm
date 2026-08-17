#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsAchievabilityChecker.h"

#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsLpChecker.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsObjectiveHelper.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm::modelchecker::multiobjective {

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsAchievabilityChecker<SparseModelType, GeometryValueType>::DeterministicSchedsAchievabilityChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>& preprocessorResult)
    : model(preprocessorResult.preprocessedModel), originalModelInitialState(*preprocessorResult.originalModel.getInitialStates().begin()) {
    for (auto const& obj : preprocessorResult.objectives) {
        objectiveHelper.emplace_back(*model, obj);
        if (!objectiveHelper.back().hasThreshold()) {
            STORM_LOG_ASSERT(!optimizingObjectiveIndex.has_value(),
                             "Unexpected input: got a (quantitative) achievability query but there are more than one objectives without a threshold.");
            optimizingObjectiveIndex = objectiveHelper.size() - 1;
        }
    }
    lpChecker = std::make_shared<DeterministicSchedsLpChecker<SparseModelType, GeometryValueType>>(*model, objectiveHelper);
}

template<class SparseModelType, typename GeometryValueType>
std::unique_ptr<CheckResult> DeterministicSchedsAchievabilityChecker<SparseModelType, GeometryValueType>::check(Environment const& env) {
    uint64_t const numObj = objectiveHelper.size();
    // Create a polytope that contains all the points that would certify achievability of the point induced by the objective threshold
    std::vector<storm::storage::geometry::Halfspace<GeometryValueType>> thresholdHalfspaces;
    std::vector<GeometryValueType> objVector;
    for (uint64_t objIndex = 0; objIndex < numObj; ++objIndex) {
        auto& obj = objectiveHelper[objIndex];
        obj.computeLowerUpperBounds(env);
        if (!optimizingObjectiveIndex.has_value() || *optimizingObjectiveIndex != objIndex) {
            objVector.assign(numObj, storm::utility::zero<GeometryValueType>());
            objVector[objIndex] = -storm::utility::one<GeometryValueType>();
            thresholdHalfspaces.emplace_back(objVector, storm::utility::convertNumber<GeometryValueType, ModelValueType>(-obj.getThreshold()));
        }
    }
    auto thresholdPolytope = storm::storage::geometry::Polytope<GeometryValueType>::create(std::move(thresholdHalfspaces));

    // Set objective weights (none if this is a qualitative achievability query)
    objVector.assign(numObj, storm::utility::zero<GeometryValueType>());
    auto eps = objVector;
    if (optimizingObjectiveIndex.has_value()) {
        objVector[*optimizingObjectiveIndex] = storm::utility::one<GeometryValueType>();
        eps[*optimizingObjectiveIndex] = env.modelchecker().multi().getPrecision() * storm::utility::convertNumber<GeometryValueType, uint64_t>(2u);
        if (env.modelchecker().multi().getPrecisionType() == MultiObjectiveModelCheckerEnvironment::PrecisionType::RelativeToDiff) {
            eps[*optimizingObjectiveIndex] *= storm::utility::convertNumber<GeometryValueType, ModelValueType>(
                objectiveHelper[*optimizingObjectiveIndex].getUpperValueBoundAtState(originalModelInitialState) -
                objectiveHelper[*optimizingObjectiveIndex].getLowerValueBoundAtState(originalModelInitialState));
        }
        STORM_LOG_INFO("Computing quantitative achievability value with precision " << eps[*optimizingObjectiveIndex] << ".");
    }
    lpChecker->setCurrentWeightVector(env, objVector);

    // Search achieving point
    auto achievingPoint = lpChecker->check(env, thresholdPolytope, eps);
    bool const isAchievable = achievingPoint.has_value();
    using ValueType = typename SparseModelType::ValueType;
    if (isAchievable) {
        STORM_LOG_INFO(
            "Found achievable point: " << storm::utility::vector::toString(achievingPoint->first) << " ( approx. "
                                       << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(achievingPoint->first))
                                       << " ).");
        if (optimizingObjectiveIndex.has_value()) {
            // Average between obtained lower- and upper bounds
            auto result =
                storm::utility::convertNumber<ValueType, GeometryValueType>(achievingPoint->first[*optimizingObjectiveIndex] + achievingPoint->second);
            result /= storm::utility::convertNumber<ValueType, uint64_t>(2u);
            if (objectiveHelper[*optimizingObjectiveIndex].minimizing()) {
                result *= -storm::utility::one<ValueType>();
            }
            return std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(originalModelInitialState, result);
        }
    }
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>>(originalModelInitialState, isAchievable);
}

template class DeterministicSchedsAchievabilityChecker<storm::models::sparse::Mdp<double>, storm::RationalNumber>;
template class DeterministicSchedsAchievabilityChecker<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber>;
template class DeterministicSchedsAchievabilityChecker<storm::models::sparse::MarkovAutomaton<double>, storm::RationalNumber>;
template class DeterministicSchedsAchievabilityChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>, storm::RationalNumber>;
}  // namespace storm::modelchecker::multiobjective
