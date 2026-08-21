#include "SparseModelValueTypeTransformer.h"

#include "storm/exceptions/IllegalArgumentTypeException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/Smg.h"
#include "storm/models/sparse/StochasticTwoPlayerGame.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::transformer {
template<typename InputValueType, typename OutputValueType>
std::shared_ptr<storm::models::sparse::Model<OutputValueType>> SparseModelValueTypeTransformer<InputValueType, OutputValueType>::transformModel(
    std::shared_ptr<storm::models::sparse::Model<InputValueType>> const& inputModel) {
    STORM_LOG_THROW(inputModel, storm::exceptions::IllegalArgumentTypeException, "Cannot transform a null model.");
    storm::storage::sparse::ModelComponents<OutputValueType> convertedComponents;
    convertedComponents.transitionMatrix = inputModel->getTransitionMatrix().template toValueType<OutputValueType>();
    convertedComponents.choiceLabeling = inputModel->getOptionalChoiceLabeling();
    convertedComponents.stateLabeling = inputModel->getStateLabeling();
    convertedComponents.stateValuations = inputModel->getOptionalStateValuations();
    convertedComponents.choiceOrigins = inputModel->getOptionalChoiceOrigins();
    for (auto const& [rewardModelName, rewardModel] : inputModel->getRewardModels()) {
        // Transform reward models
        std::optional<std::vector<OutputValueType>> optionalStateRewardVector = std::nullopt;
        std::optional<std::vector<OutputValueType>> optionalStateActionRewardVector = std::nullopt;
        std::optional<storm::storage::SparseMatrix<OutputValueType>> optionalTransitionRewardMatrix = std::nullopt;
        if (rewardModel.hasStateRewards()) {
            std::vector<OutputValueType> resultVector;
            resultVector.reserve(rewardModel.getStateRewardVector().size());
            for (auto const& oldValue : rewardModel.getStateRewardVector()) {
                resultVector.push_back(storm::utility::convertNumber<OutputValueType>(oldValue));
            }
            optionalStateRewardVector = resultVector;
        }
        if (rewardModel.hasStateActionRewards()) {
            std::vector<OutputValueType> resultVector;
            resultVector.reserve(rewardModel.getStateActionRewardVector().size());
            for (auto const& oldValue : rewardModel.getStateActionRewardVector()) {
                resultVector.push_back(storm::utility::convertNumber<OutputValueType>(oldValue));
            }
            optionalStateActionRewardVector = resultVector;
        }
        if (rewardModel.hasTransitionRewards()) {
            optionalTransitionRewardMatrix = rewardModel.getTransitionRewardMatrix().template toValueType<OutputValueType>();
        }
        convertedComponents.rewardModels.emplace(
            rewardModelName, storm::models::sparse::StandardRewardModel<OutputValueType>(
                                 std::move(optionalStateRewardVector), std::move(optionalStateActionRewardVector), std::move(optionalTransitionRewardMatrix)));
    }
    switch (inputModel->getType()) {
        case storm::models::ModelType::Dtmc:
            return std::make_shared<storm::models::sparse::Dtmc<OutputValueType>>(storm::models::sparse::Dtmc<OutputValueType>(convertedComponents));
        case storm::models::ModelType::Mdp:
            return std::make_shared<storm::models::sparse::Mdp<OutputValueType>>(storm::models::sparse::Mdp<OutputValueType>(convertedComponents));
        case storm::models::ModelType::Ctmc: {
            auto ctmc = inputModel->template as<storm::models::sparse::Ctmc<InputValueType>>();
            std::vector<OutputValueType> resultVector;
            resultVector.reserve(ctmc->getExitRateVector().size());
            for (auto const& oldValue : ctmc->getExitRateVector()) {
                resultVector.push_back(storm::utility::convertNumber<OutputValueType>(oldValue));
            }
            convertedComponents.exitRates = resultVector;
            // Markov automata store probabilities in their transition matrix and rates separately in exitRates.
            convertedComponents.rateTransitions = false;
            return std::make_shared<storm::models::sparse::Ctmc<OutputValueType>>(storm::models::sparse::Ctmc<OutputValueType>(convertedComponents));
        }
        case storm::models::ModelType::MarkovAutomaton: {
            auto ma = inputModel->template as<storm::models::sparse::MarkovAutomaton<InputValueType>>();
            std::vector<OutputValueType> resultVector;
            resultVector.reserve(ma->getExitRates().size());
            for (auto const& oldValue : ma->getExitRates()) {
                resultVector.push_back(storm::utility::convertNumber<OutputValueType>(oldValue));
            }
            convertedComponents.exitRates = resultVector;
            convertedComponents.rateTransitions = true;
            convertedComponents.markovianStates = ma->getMarkovianStates();
            return std::make_shared<storm::models::sparse::MarkovAutomaton<OutputValueType>>(
                storm::models::sparse::MarkovAutomaton<OutputValueType>(convertedComponents));
        }
        case storm::models::ModelType::Pomdp: {
            auto pomdp = inputModel->template as<storm::models::sparse::Pomdp<InputValueType>>();
            convertedComponents.observabilityClasses = pomdp->getObservations();
            convertedComponents.observationValuations = pomdp->getOptionalObservationValuations();
            return std::make_shared<models::sparse::Pomdp<OutputValueType>>(models::sparse::Pomdp<OutputValueType>(convertedComponents, pomdp->isCanonic()));
        }
        case storm::models::ModelType::Smg: {
            auto smg = inputModel->template as<storm::models::sparse::Smg<InputValueType>>();
            convertedComponents.statePlayerIndications = smg->getStatePlayerIndications();
            convertedComponents.playerNameToIndexMap = smg->getPlayerNamesToIndex();
            return std::make_shared<storm::models::sparse::Smg<OutputValueType>>(models::sparse::Smg<OutputValueType>(convertedComponents));
        }
        case storm::models::ModelType::S2pg: {
            auto s2pg = inputModel->template as<storm::models::sparse::StochasticTwoPlayerGame<InputValueType>>();
            convertedComponents.player1Matrix = s2pg->getPlayer1Matrix();
            return std::make_shared<storm::models::sparse::StochasticTwoPlayerGame<OutputValueType>>(
                models::sparse::StochasticTwoPlayerGame<OutputValueType>(convertedComponents));
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentTypeException,
                            "Value type transformation is not supported for models of type " << inputModel->getType() << ".");
    }
    return nullptr;
}

template class SparseModelValueTypeTransformer<double, storm::RationalNumber>;
template class SparseModelValueTypeTransformer<double, double>;
template class SparseModelValueTypeTransformer<storm::RationalNumber, double>;
template class SparseModelValueTypeTransformer<storm::RationalNumber, storm::RationalNumber>;

}  // namespace storm::transformer
