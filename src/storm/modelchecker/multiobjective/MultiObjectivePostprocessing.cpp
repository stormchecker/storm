#include "storm/modelchecker/multiobjective/MultiObjectivePostprocessing.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<typename ValueType, typename GeometryValueType>
GeometryValueType transformObjectiveValueToOriginal(Objective<ValueType> const& objective, GeometryValueType const& value) {
    if (storm::solver::maximize(objective.formula->getOptimalityType())) {
        if (objective.considersComplementaryEvent) {
            return storm::utility::one<GeometryValueType>() - value;
        } else {
            return value;
        }
    } else {
        if (objective.considersComplementaryEvent) {
            return storm::utility::one<GeometryValueType>() + value;
        } else {
            return -value;
        }
    }
}

template<typename ValueType, typename GeometryValueType>
std::vector<GeometryValueType> transformObjectiveValuesToOriginal(std::vector<Objective<ValueType>> const& objectives,
                                                                  std::vector<GeometryValueType> const& point) {
    std::vector<GeometryValueType> result;
    result.reserve(point.size());
    for (uint_fast64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        result.push_back(transformObjectiveValueToOriginal(objectives[objIndex], point[objIndex]));
    }
    return result;
}

template<typename ValueType, typename GeometryValueType>
std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>> transformObjectivePolytopeToOriginal(
    std::vector<Objective<ValueType>> const& objectives, std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>> const& polytope) {
    if (polytope->isEmpty()) {
        return storm::storage::geometry::Polytope<GeometryValueType>::createEmptyPolytope();
    }
    if (polytope->isUniversal()) {
        return storm::storage::geometry::Polytope<GeometryValueType>::createUniversalPolytope();
    }
    uint_fast64_t numObjectives = objectives.size();
    std::vector<std::vector<GeometryValueType>> transformationMatrix(numObjectives,
                                                                     std::vector<GeometryValueType>(numObjectives, storm::utility::zero<GeometryValueType>()));
    std::vector<GeometryValueType> transformationVector;
    transformationVector.reserve(numObjectives);
    for (uint_fast64_t objIndex = 0; objIndex < numObjectives; ++objIndex) {
        auto const& obj = objectives[objIndex];
        if (storm::solver::maximize(obj.formula->getOptimalityType())) {
            if (obj.considersComplementaryEvent) {
                transformationMatrix[objIndex][objIndex] = -storm::utility::one<GeometryValueType>();
                transformationVector.push_back(storm::utility::one<GeometryValueType>());
            } else {
                transformationMatrix[objIndex][objIndex] = storm::utility::one<GeometryValueType>();
                transformationVector.push_back(storm::utility::zero<GeometryValueType>());
            }
        } else {
            if (obj.considersComplementaryEvent) {
                transformationMatrix[objIndex][objIndex] = storm::utility::one<GeometryValueType>();
                transformationVector.push_back(storm::utility::one<GeometryValueType>());
            } else {
                transformationMatrix[objIndex][objIndex] = -storm::utility::one<GeometryValueType>();
                transformationVector.push_back(storm::utility::zero<GeometryValueType>());
            }
        }
    }
    return polytope->affineTransformation(transformationMatrix, transformationVector);
}

/*
 * This function is only responsible to reverse changes to the model made in the preprocessor
 * (not the ones done by specific checkers)
 */
template<typename ValueType>
void transformObjectiveSchedulersToOriginal(storm::storage::SparseModelMemoryProductReverseData const& reverseData,
                                            std::vector<storm::storage::Scheduler<ValueType>>& schedulers) {
    for (auto& currScheduler : schedulers) {
        currScheduler = reverseData.createMemorySchedulerFromProductScheduler(currScheduler);
    }
}

template storm::RationalNumber transformObjectiveValueToOriginal(Objective<double> const& objective, storm::RationalNumber const& value);
template std::vector<storm::RationalNumber> transformObjectiveValuesToOriginal(std::vector<Objective<double>> const& objectives,
                                                                               std::vector<storm::RationalNumber> const& point);
template std::shared_ptr<storm::storage::geometry::Polytope<storm::RationalNumber>> transformObjectivePolytopeToOriginal(
    std::vector<Objective<double>> const& objectives, std::shared_ptr<storm::storage::geometry::Polytope<storm::RationalNumber>> const& polytope);

template storm::RationalNumber transformObjectiveValueToOriginal(Objective<storm::RationalNumber> const& objective, storm::RationalNumber const& value);
template std::vector<storm::RationalNumber> transformObjectiveValuesToOriginal(std::vector<Objective<storm::RationalNumber>> const& objectives,
                                                                               std::vector<storm::RationalNumber> const& point);
template std::shared_ptr<storm::storage::geometry::Polytope<storm::RationalNumber>> transformObjectivePolytopeToOriginal(
    std::vector<Objective<storm::RationalNumber>> const& objectives,
    std::shared_ptr<storm::storage::geometry::Polytope<storm::RationalNumber>> const& polytope);

template void transformObjectiveSchedulersToOriginal(storm::storage::SparseModelMemoryProductReverseData const& reverseData,
                                                     std::vector<storm::storage::Scheduler<double>>& schedulers);

template void transformObjectiveSchedulersToOriginal(storm::storage::SparseModelMemoryProductReverseData const& reverseData,
                                                     std::vector<storm::storage::Scheduler<storm::RationalNumber>>& schedulers);

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm