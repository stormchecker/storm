#pragma once

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/helper/SspParetoFront.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
class SparseSspCvarParetoViHelper {
   public:
    SparseSspCvarParetoViHelper(CvarQueryInformation const& queryInformation,
                                preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : queryInformation(queryInformation), preprocessingResult(preprocessingResult) {
        // Intentionally left empty.
    }

    CvarComputationResult<ValueType> computeCvar(Environment const&, bool produceScheduler = false) const {
        static_cast<void>(produceScheduler);
        static_assert(sizeof(SspParetoFront<ValueType>) > 0, "Expected SSP Pareto front type to be available.");
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "CVaR for stochastic shortest path objectives is not implemented yet.");
    }

   private:
    CvarQueryInformation const& queryInformation;
    preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
