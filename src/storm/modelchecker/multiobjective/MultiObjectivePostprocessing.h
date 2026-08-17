#pragma once

#include <memory>
#include <vector>

#include "storm/modelchecker/multiobjective/Objective.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/geometry/Polytope.h"
#include "storm/storage/memorystructure/SparseModelMemoryProductReverseData.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<typename ValueType, typename GeometryValueType>
GeometryValueType transformObjectiveValueToOriginal(Objective<ValueType> const& objectives, GeometryValueType const& value);

template<typename ValueType, typename GeometryValueType>
std::vector<GeometryValueType> transformObjectiveValuesToOriginal(std::vector<Objective<ValueType>> const& objectives,
                                                                  std::vector<GeometryValueType> const& point);

template<typename ValueType, typename GeometryValueType>
std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>> transformObjectivePolytopeToOriginal(
    std::vector<Objective<ValueType>> const& objectives, std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>> const& polytope);

/*
 * Uses the information from reverse data to edit the objective schedulers in 'schedulers' to ones
 * apt for the target model.
 */
template<typename ValueType>
void transformObjectiveSchedulersToOriginal(storm::storage::SparseModelMemoryProductReverseData const& reverseData,
                                            std::vector<storm::storage::Scheduler<ValueType>>& schedulers);

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm