#pragma once

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarModelCheckingData.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
class SparseCvarHelper {
   public:
    explicit SparseCvarHelper(CvarModelCheckingData<SparseMdpModelType> const& modelCheckingData) : modelCheckingData(modelCheckingData) {
        // Intentionally left empty.
    }

    typename SparseMdpModelType::ValueType computeCvar(Environment const&) const {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "CVaR model checking for sparse MDPs is not implemented yet after validating terminal reward model '"
                            << modelCheckingData.rewardModelName << "'.");
    }

   private:
    CvarModelCheckingData<SparseMdpModelType> const& modelCheckingData;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
