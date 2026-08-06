#include "storm/transformer/StatePermuter.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"
#include "storm/utility/permutation.h"
#include "storm/utility/vector.h"

namespace storm::transformer {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> permuteStates(storm::models::sparse::Model<ValueType> const& originalModel,
                                                                       std::vector<uint64_t> const& permutation) {
    STORM_LOG_ASSERT(originalModel.getNumberOfStates() == permutation.size(), "Invalid permutation size.");
    STORM_LOG_ASSERT(storm::utility::permutation::isValidPermutation(permutation), "Invalid permutation.");
    auto const inversePermutation = storm::utility::permutation::invertPermutation(permutation);
    storm::OptionalRef<std::vector<storm::storage::SparseMatrixIndexType> const> optionalGroupIndices;
    if (!originalModel.getTransitionMatrix().hasTrivialRowGrouping()) {
        optionalGroupIndices.reset(originalModel.getTransitionMatrix().getRowGroupIndices());
    }

    // transitions, state labels, reward models
    auto permutedMatrix = originalModel.getTransitionMatrix().permuteRowGroupsAndColumns(inversePermutation, permutation);
    auto permutedStateLabels = originalModel.getStateLabeling();
    permutedStateLabels.permuteItems(inversePermutation);
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(permutedMatrix), std::move(permutedStateLabels));
    for (auto const& [name, origRewardModel] : originalModel.getRewardModels()) {
        auto permutedRewardModel = origRewardModel.permuteStates(inversePermutation, optionalGroupIndices, permutation);
        components.rewardModels.emplace(name, std::move(permutedRewardModel));
    }

    // Choice labeling, choice origins, state valuations
    if (originalModel.hasChoiceLabeling()) {
        auto permutedChoiceLabeling = originalModel.getChoiceLabeling();
        permutedChoiceLabeling.permuteItems(inversePermutation);
        components.choiceLabeling = std::move(permutedChoiceLabeling);
    }
    STORM_LOG_WARN_COND(!originalModel.hasStateValuations(), "State valuations will be dropped as permuting them is currently not implemented.");
    STORM_LOG_WARN_COND(!originalModel.hasChoiceOrigins(), "Choice origins will be dropped as permuting them is currently not implemented.");

    // Model type specific components
    if (originalModel.isOfType(storm::models::ModelType::Pomdp)) {
        auto const& pomdp = *originalModel.template as<storm::models::sparse::Pomdp<ValueType>>();
        components.observabilityClasses = storm::utility::vector::applyInversePermutation(inversePermutation, pomdp.getObservations());
        STORM_LOG_WARN_COND(!pomdp.hasObservationValuations(), "Observation valuations will be dropped as permuting them is currently not implemented.");
    }
    if (originalModel.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        auto const& ma = *originalModel.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        components.markovianStates = ma.getMarkovianStates().permute(inversePermutation);
        components.exitRates = storm::utility::vector::applyInversePermutation(inversePermutation, ma.getExitRates());
        components.rateTransitions = false;  // Note that originalModel.getTransitionMatrix() contains probabilities
    } else if (originalModel.isOfType(storm::models::ModelType::Ctmc)) {
        auto const& ctmc = *originalModel.template as<storm::models::sparse::Ctmc<ValueType>>();
        components.exitRates = storm::utility::vector::applyInversePermutation(inversePermutation, ctmc.getExitRateVector());
        components.rateTransitions = true;
    } else {
        STORM_LOG_THROW(originalModel.isOfType(storm::models::ModelType::Dtmc) || originalModel.isOfType(storm::models::ModelType::Mdp),
                        storm::exceptions::UnexpectedException, "Unhandled model type.");
    }
    return storm::utility::builder::buildModelFromComponents(originalModel.getType(), std::move(components));
}

template std::shared_ptr<storm::models::sparse::Model<double>> permuteStates(storm::models::sparse::Model<double> const& originalModel,
                                                                             std::vector<uint64_t> const& permutation);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> permuteStates(
    storm::models::sparse::Model<storm::RationalNumber> const& originalModel, std::vector<uint64_t> const& permutation);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> permuteStates(
    storm::models::sparse::Model<storm::RationalFunction> const& originalModel, std::vector<uint64_t> const& permutation);
template std::shared_ptr<storm::models::sparse::Model<storm::Interval>> permuteStates(storm::models::sparse::Model<storm::Interval> const& originalModel,
                                                                                      std::vector<uint64_t> const& permutation);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> permuteStates(
    storm::models::sparse::Model<storm::RationalInterval> const& originalModel, std::vector<uint64_t> const& permutation);
}  // namespace storm::transformer