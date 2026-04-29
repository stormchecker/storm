#pragma once

#include "storm/environment/Environment.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/SparseWeightedReachabilityCvarLpHelper.h"
#include "storm/modelchecker/cvar/WeightedReachabilityCvarLpData.h"
#include "storm/modelchecker/cvar/preprocessing/WeightedReachabilityCvarPreprocessor.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
CvarComputationResult<typename SparseMdpModelType::ValueType> computeWeightedReachabilityCvar(
    Environment const& env, SparseMdpModelType const& model, CvarQueryInformation const& queryInformation, storm::storage::BitVector const& targetStates,
    bool produceScheduler = false) {
    auto weightedReachabilityPreprocessingResult =
        preprocessing::preprocessWeightedReachabilityCvar(model, queryInformation, targetStates, produceScheduler);
    auto weightedReachabilityCvarLpData = createWeightedReachabilityCvarLpData(queryInformation, weightedReachabilityPreprocessingResult);

    SparseWeightedReachabilityCvarLpHelper<typename SparseMdpModelType::ValueType> cvarHelper(weightedReachabilityCvarLpData);
    return cvarHelper.computeCvar(env, produceScheduler);
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
