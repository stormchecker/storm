#include "storm-parsers/parser/MarkovAutomatonParser.h"

#include "storm-parsers/parser/NondeterministicSparseTransitionParser.h"
#include "storm-parsers/parser/SparseItemLabelingParser.h"
#include "storm-parsers/parser/SparseStateRewardParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace storm {
namespace parser {

template<typename ValueType, typename RewardValueType>
storm::models::sparse::MarkovAutomaton<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
MarkovAutomatonParser<ValueType, RewardValueType>::parseMarkovAutomaton(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                        std::string const& stateRewardFilename, std::string const& transitionRewardFilename,
                                                                        std::string const& choiceLabelingFilename, ExplicitModelParserOptions const& options) {
    // Parse the transitions of the Markov Automaton.
    typename storm::parser::MarkovAutomatonSparseTransitionParser<ValueType>::Result transitionResult(
        storm::parser::MarkovAutomatonSparseTransitionParser<ValueType>::parseMarkovAutomatonTransitions(transitionsFilename, options));

    // Build the actual transition matrix using the MatrixBuilder provided by the transitionResult.
    storm::storage::SparseMatrix<ValueType> transitionMatrix(transitionResult.transitionMatrixBuilder.build());

    // Parse the state labeling.
    storm::models::sparse::StateLabeling resultLabeling(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(transitionMatrix.getColumnCount(), labelingFilename));

    // Construct the result components
    storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> components(std::move(transitionMatrix),
                                                                                                                               std::move(resultLabeling));
    components.rateTransitions = true;
    components.markovianStates = std::move(transitionResult.markovianStates);
    components.exitRates = std::move(transitionResult.exitRates);

    // If given, parse the state rewards file.
    std::optional<std::vector<RewardValueType>> stateRewards;

    if (!stateRewardFilename.empty()) {
        stateRewards.emplace(
            storm::parser::SparseStateRewardParser<RewardValueType>::parseSparseStateReward(components.transitionMatrix.getColumnCount(), stateRewardFilename));
    }

    // Only parse transition rewards if a file is given.
    std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards;
    if (!transitionRewardFilename.empty()) {
        transitionRewards = std::move(storm::parser::NondeterministicSparseTransitionParser<RewardValueType>::parseNondeterministicTransitionRewards(
            transitionRewardFilename, components.transitionMatrix));
    }

    if (stateRewards || transitionRewards) {
        components.rewardModels.insert(std::make_pair(
            "", storm::models::sparse::StandardRewardModel<RewardValueType>(std::move(stateRewards), std::nullopt, std::move(transitionRewards))));
    }

    // Only parse choice labeling if a file is given.
    std::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;
    if (!choiceLabelingFilename.empty()) {
        components.choiceLabeling = storm::parser::SparseItemLabelingParser::parseChoiceLabeling(
            components.transitionMatrix.getRowCount(), choiceLabelingFilename, components.transitionMatrix.getRowGroupIndices());
    }

    // generate the Markov Automaton.
    return storm::models::sparse::MarkovAutomaton<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(std::move(components));
}

template class MarkovAutomatonParser<double, double>;
template class MarkovAutomatonParser<double, storm::Interval>;

}  // namespace parser
}  // namespace storm
