#include "storm/transformer/ChoiceSelector.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace transformer {
template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::NondeterministicModel<ValueType, RewardModelType>> ChoiceSelector<ValueType, RewardModelType>::transform(
    storm::storage::BitVector const& enabledActions) const {
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> newComponents(inputModel.getTransitionMatrix().restrictRows(enabledActions));
    newComponents.stateLabeling = inputModel.getStateLabeling();
    for (auto const& rewardModel : inputModel.getRewardModels()) {
        newComponents.rewardModels.emplace(rewardModel.first, rewardModel.second.restrictActions(enabledActions));
    }
    if (inputModel.hasChoiceLabeling()) {
        newComponents.choiceLabeling = inputModel.getChoiceLabeling().getSubLabeling(enabledActions);
    }
    newComponents.stateValuations = inputModel.getOptionalStateValuations();
    if (inputModel.hasChoiceOrigins()) {
        newComponents.choiceOrigins = inputModel.getChoiceOrigins()->selectChoices(enabledActions);
    }

    if (inputModel.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        auto const& ma = *inputModel.template as<storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType>>();
        newComponents.markovianStates = ma.getMarkovianStates();
        newComponents.exitRates = ma.getExitRates();
        return std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType>>(std::move(newComponents));
    } else if (inputModel.getType() == storm::models::ModelType::Pomdp) {
        newComponents.observabilityClasses = static_cast<storm::models::sparse::Pomdp<ValueType, RewardModelType> const&>(inputModel).getObservations();
        return std::make_shared<storm::models::sparse::Pomdp<ValueType, RewardModelType>>(std::move(newComponents));
    } else {
        STORM_LOG_THROW(inputModel.isOfType(storm::models::ModelType::Mdp), storm::exceptions::UnexpectedException,
                        "Unexpected model type for choice selector.");
        return std::make_shared<storm::models::sparse::Mdp<ValueType, RewardModelType>>(std::move(newComponents));
    }
}

template class ChoiceSelector<double>;
template class ChoiceSelector<storm::RationalNumber>;
}  // namespace transformer
}  // namespace storm
