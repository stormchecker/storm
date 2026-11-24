#include "SparseRationalModelToDoubleTransformer.h"

#include "storm/api/builder.h"
#include "storm/exceptions/IllegalArgumentTypeException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/Smg.h"
#include "storm/models/sparse/StochasticTwoPlayerGame.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::transformer {
std::shared_ptr<storm::models::sparse::Model<double>> sparseRationalModelToDouble(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> const& inputModel, double precision) {
    STORM_LOG_THROW(inputModel, storm::exceptions::IllegalArgumentTypeException, "Cannot transform a null model.");
    storm::storage::sparse::ModelComponents<double> convertedComponents;
    convertedComponents.transitionMatrix = inputModel->getTransitionMatrix().toValueType<double>();
    // For models with probability transition matrices, we scale rows that do not sum up to 1 (within the given precision) to mitigate numerical errors
    if (inputModel->getType() != storm::models::ModelType::Ctmc) {
        for (uint64_t row = 0; row < convertedComponents.transitionMatrix.getRowCount(); ++row) {
            auto const rowSum = convertedComponents.transitionMatrix.getRowSum(row);
            if (!storm::utility::isApproxEqual(rowSum, 1.0, precision, false)) {
                for (auto& entry : convertedComponents.transitionMatrix.getRow(row)) {
                    entry.setValue(entry.getValue() / rowSum);
                }
            }
        }
    }
    convertedComponents.choiceLabeling = inputModel->getOptionalChoiceLabeling();
    convertedComponents.stateLabeling = inputModel->getStateLabeling();
    convertedComponents.stateValuations = inputModel->getOptionalStateValuations();
    convertedComponents.choiceOrigins = inputModel->getOptionalChoiceOrigins();
    for (auto const& [rewardModelName, rewardModel] : inputModel->getRewardModels()) {
        // Transform reward models
        std::optional<std::vector<double>> optionalStateRewardVector = std::nullopt;
        std::optional<std::vector<double>> optionalStateActionRewardVector = std::nullopt;
        std::optional<storm::storage::SparseMatrix<double>> optionalTransitionRewardMatrix = std::nullopt;
        if (rewardModel.hasStateRewards()) {
            optionalStateRewardVector = storm::utility::vector::convertNumericVector<double>(rewardModel.getStateRewardVector());
        }
        if (rewardModel.hasStateActionRewards()) {
            optionalStateActionRewardVector = storm::utility::vector::convertNumericVector<double>(rewardModel.getStateActionRewardVector());
        }
        if (rewardModel.hasTransitionRewards()) {
            optionalTransitionRewardMatrix = rewardModel.getTransitionRewardMatrix().toValueType<double>();
        }
        convertedComponents.rewardModels.emplace(
            rewardModelName, storm::models::sparse::StandardRewardModel<double>(
                                 std::move(optionalStateRewardVector), std::move(optionalStateActionRewardVector), std::move(optionalTransitionRewardMatrix)));
    }

    // Handle special model-type-specific components
    switch (inputModel->getType()) {
        // For types that do not need additional components.
        // We still add them explicitly to catch model types that the transformer does not support.
        case storm::models::ModelType::Dtmc:
        case storm::models::ModelType::Mdp:
            break;
        case storm::models::ModelType::Ctmc: {
            convertedComponents.rateTransitions = true;
            break;
        }
        case storm::models::ModelType::MarkovAutomaton: {
            auto ma = inputModel->as<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>();
            std::vector<double> resultVector;
            resultVector.reserve(ma->getExitRates().size());
            for (auto const& oldValue : ma->getExitRates()) {
                resultVector.push_back(storm::utility::convertNumber<double>(oldValue));
            }
            convertedComponents.exitRates = resultVector;
            // Markov automata store probabilities in their transition matrix and rates separately in exitRates.
            convertedComponents.rateTransitions = false;
            convertedComponents.markovianStates = ma->getMarkovianStates();
            break;
        }
        case storm::models::ModelType::Pomdp: {
            auto pomdp = inputModel->as<storm::models::sparse::Pomdp<storm::RationalNumber>>();
            convertedComponents.observabilityClasses = pomdp->getObservations();
            convertedComponents.observationValuations = pomdp->getOptionalObservationValuations();
            break;
        }
        case storm::models::ModelType::Smg: {
            auto smg = inputModel->as<storm::models::sparse::Smg<storm::RationalNumber>>();
            convertedComponents.statePlayerIndications = smg->getStatePlayerIndications();
            convertedComponents.playerNameToIndexMap = smg->getPlayerNamesToIndex();
            break;
        }
        case storm::models::ModelType::S2pg: {
            auto s2pg = inputModel->as<storm::models::sparse::StochasticTwoPlayerGame<storm::RationalNumber>>();
            convertedComponents.player1Matrix = s2pg->getPlayer1Matrix();
            break;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Transformation to double is not supported for models of type " << inputModel->getType() << ".");
    }
    return storm::utility::builder::buildModelFromComponents(inputModel->getType(), std::move(convertedComponents));
}
}  // namespace storm::transformer
