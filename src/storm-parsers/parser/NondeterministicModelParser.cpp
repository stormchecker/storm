#include "storm-parsers/parser/NondeterministicModelParser.h"

#include <string>
#include <vector>

#include "storm-parsers/parser/NondeterministicSparseTransitionParser.h"
#include "storm-parsers/parser/SparseItemLabelingParser.h"
#include "storm-parsers/parser/SparseStateRewardParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm {
namespace parser {

template<typename ValueType, typename RewardValueType>
storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
NondeterministicModelParser<ValueType, RewardValueType>::parseNondeterministicModel(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                                    std::string const& stateRewardFilename,
                                                                                    std::string const& transitionRewardFilename,
                                                                                    std::string const& choiceLabelingFilename,
                                                                                    ExplicitModelParserOptions const& options) {
    // Parse the transitions.
    storm::storage::SparseMatrix<ValueType> transitions(
        storm::parser::NondeterministicSparseTransitionParser<ValueType>::parseNondeterministicTransitions(transitionsFilename, options));

    uint_fast64_t stateCount = transitions.getColumnCount();

    // Parse the state labeling.
    storm::models::sparse::StateLabeling labeling(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(stateCount, labelingFilename));

    // Initialize result.
    storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> result(std::move(transitions),
                                                                                                                           std::move(labeling));

    // Only parse state rewards if a file is given.
    std::optional<std::vector<RewardValueType>> stateRewards;
    if (!stateRewardFilename.empty()) {
        stateRewards = std::move(storm::parser::SparseStateRewardParser<RewardValueType>::parseSparseStateReward(stateCount, stateRewardFilename));
    }

    // Only parse transition rewards if a file is given.
    std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards;
    if (!transitionRewardFilename.empty()) {
        transitionRewards = std::move(storm::parser::NondeterministicSparseTransitionParser<RewardValueType>::parseNondeterministicTransitionRewards(
            transitionRewardFilename, result.transitionMatrix));
    }

    if (stateRewards || transitionRewards) {
        result.rewardModels.insert(std::make_pair(
            "", storm::models::sparse::StandardRewardModel<RewardValueType>(std::move(stateRewards), std::nullopt, std::move(transitionRewards))));
    }

    // Only parse choice labeling if a file is given.
    std::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;
    if (!choiceLabelingFilename.empty()) {
        result.choiceLabeling = storm::parser::SparseItemLabelingParser::parseChoiceLabeling(result.transitionMatrix.getRowCount(), choiceLabelingFilename,
                                                                                             result.transitionMatrix.getRowGroupIndices());
    }

    return result;
}

template<typename ValueType, typename RewardValueType>
storm::models::sparse::Mdp<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
NondeterministicModelParser<ValueType, RewardValueType>::parseMdp(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                  std::string const& stateRewardFilename, std::string const& transitionRewardFilename,
                                                                  std::string const& choiceLabelingFilename, ExplicitModelParserOptions const& options) {
    auto parserResult =
        parseNondeterministicModel(transitionsFilename, labelingFilename, stateRewardFilename, transitionRewardFilename, choiceLabelingFilename, options);

    return storm::models::sparse::Mdp<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(std::move(parserResult));
}

template class NondeterministicModelParser<double, double>;
template class NondeterministicModelParser<double, storm::Interval>;

} /* namespace parser */
} /* namespace storm */
