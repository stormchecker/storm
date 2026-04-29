#pragma once

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessor.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
CvarComputationResult<typename SparseMdpModelType::ValueType> computeSspCvar(Environment const&, SparseMdpModelType const& model,
                                                                              CvarQueryInformation const& queryInformation,
                                                                              storm::storage::BitVector const& targetStates,
                                                                              bool produceScheduler = false) {
    auto sspPreprocessingResult = preprocessing::preprocessSspCvar(model, queryInformation, targetStates);
    static_cast<void>(sspPreprocessingResult);
    static_cast<void>(produceScheduler);
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "CVaR for stochastic shortest path objectives is not implemented yet.");
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
