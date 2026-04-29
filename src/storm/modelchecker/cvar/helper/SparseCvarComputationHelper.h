#pragma once

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/cvar/CvarClassification.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/helper/SparseWeightedReachabilityCvarLpHelper.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessor.h"
#include "storm/modelchecker/cvar/preprocessing/WeightedReachabilityCvarPreprocessor.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
class SparseCvarComputationHelper {
   public:
    using ValueType = typename SparseMdpModelType::ValueType;

    SparseCvarComputationHelper(SparseMdpModelType const& model, CvarQueryInformation const& queryInformation,
                                storm::storage::BitVector const& targetStates)
        : model(model), queryInformation(queryInformation), targetStates(targetStates) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const& env, bool produceScheduler = false) const {
        auto queryKind = classifyCvarQuery(queryInformation);
        auto backendKind = selectCvarBackend(model, queryInformation, queryKind, targetStates, env.modelchecker().cvar().getMethod());

        switch (backendKind) {
            case CvarBackendKind::WeightedReachability: {
                auto weightedReachabilityPreprocessingResult =
                    preprocessing::preprocessWeightedReachabilityCvar(model, queryInformation, targetStates, produceScheduler);
                SparseWeightedReachabilityCvarLpHelper<ValueType> cvarHelper(queryInformation, weightedReachabilityPreprocessingResult);
                return cvarHelper.computeCvar(env, produceScheduler);
            }
            case CvarBackendKind::Ssp: {
                auto sspPreprocessingResult = preprocessing::preprocessSspCvar(model, queryInformation, targetStates);
                static_cast<void>(sspPreprocessingResult);
                static_cast<void>(produceScheduler);
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                                "CVaR for stochastic shortest path objectives is not implemented yet.");
            }
        }
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Encountered an unknown CVaR backend.");
    }

   private:
    SparseMdpModelType const& model;
    CvarQueryInformation const& queryInformation;
    storm::storage::BitVector const& targetStates;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
