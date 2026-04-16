#pragma once

#include <string>

#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
#include "storm/modelchecker/cvar/WeightedReachabilityModelInformation.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
struct CvarModelCheckingData {
    using ValueType = typename SparseMdpModelType::ValueType;

    double alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    uint64_t initialState;
    std::string rewardModelName;
    storm::storage::BitVector targetStates;
    std::vector<ValueType> terminalRewards;
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    SparseMdpModelType const& model;
};

template<typename SparseMdpModelType>
CvarModelCheckingData<SparseMdpModelType> createCvarModelCheckingData(
    SparseMdpModelType const& model, CvarFormulaInformation const& formulaInformation,
    WeightedReachabilityModelInformation<typename SparseMdpModelType::ValueType> const& weightedReachabilityModelInformation) {
    return {formulaInformation.alpha,
            formulaInformation.optimizationDirection,
            *model.getInitialStates().begin(),
            weightedReachabilityModelInformation.rewardModelName,
            weightedReachabilityModelInformation.targetStates,
            weightedReachabilityModelInformation.terminalRewards,
            model.getTransitionMatrix(),
            model};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
