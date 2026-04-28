#pragma once

#include <string>
#include <vector>

#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

/*!
 * Collects the normalized SSP model information needed by the future CVaR VI.
 *
 * The SSP CVaR algorithm works with per-choice costs. If the input reward model
 * only uses state rewards, we lift those rewards to equivalent state-action
 * costs by copying the state reward to each outgoing choice of the state.
 */
template<typename ValueType>
struct SspModelInformation {
    std::string rewardModelName;
    uint64_t initialState;
    storm::storage::BitVector targetStates;
    bool liftedStateRewardsToChoiceCosts;
    std::vector<ValueType> choiceCosts;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

template<typename SparseMdpModelType>
SspModelInformation<typename SparseMdpModelType::ValueType> extractSspModelInformation(SparseMdpModelType const& model,
                                                                                        CvarFormulaInformation const& formulaInformation,
                                                                                        storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::string rewardModelName = formulaInformation.rewardModelName ? formulaInformation.rewardModelName.get() : "";
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                    "CVaR SSP preprocessing does not support transition rewards.");

    bool liftedStateRewardsToChoiceCosts = rewardModel.hasStateRewards() && !rewardModel.hasStateActionRewards();
    if (liftedStateRewardsToChoiceCosts) {
        STORM_LOG_INFO("CVaR SSP preprocessing lifts state rewards to equivalent per-choice costs.");
    }

    auto transitionMatrix = model.getTransitionMatrix();
    auto choiceCosts = rewardModel.getTotalRewardVector(transitionMatrix);

    return {rewardModelName,
            *model.getInitialStates().begin(),
            targetStates,
            liftedStateRewardsToChoiceCosts,
            std::move(choiceCosts),
            std::move(transitionMatrix)};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
