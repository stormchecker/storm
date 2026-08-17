#include "storm-parsers/parser/DeterministicModelParser.h"

#include <string>
#include <vector>

#include "storm-parsers/parser/DeterministicSparseTransitionParser.h"
#include "storm-parsers/parser/SparseItemLabelingParser.h"
#include "storm-parsers/parser/SparseStateRewardParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm {
namespace parser {

template<typename ValueType, typename RewardValueType>
storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
DeterministicModelParser<ValueType, RewardValueType>::parseDeterministicModel(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                              std::string const& stateRewardFilename,
                                                                              std::string const& transitionRewardFilename,
                                                                              std::string const& choiceLabelingFilename,
                                                                              ExplicitModelParserOptions const& options) {
    // Parse the transitions.
    storm::storage::SparseMatrix<ValueType> transitions(
        storm::parser::DeterministicSparseTransitionParser<ValueType>::parseDeterministicTransitions(transitionsFilename, options));

    uint_fast64_t stateCount = transitions.getColumnCount();

    // Parse the state labeling.
    storm::models::sparse::StateLabeling labeling(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(stateCount, labelingFilename));

    // Construct the result.
    storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> result(std::move(transitions),
                                                                                                                           std::move(labeling));

    // Only parse state rewards if a file is given.
    std::optional<std::vector<RewardValueType>> stateRewards;
    if (stateRewardFilename != "") {
        stateRewards = storm::parser::SparseStateRewardParser<RewardValueType>::parseSparseStateReward(stateCount, stateRewardFilename);
    }

    // Only parse transition rewards if a file is given.
    std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards;
    if (transitionRewardFilename != "") {
        transitionRewards = storm::parser::DeterministicSparseTransitionParser<RewardValueType>::parseDeterministicTransitionRewards(transitionRewardFilename,
                                                                                                                                     result.transitionMatrix);
    }

    if (stateRewards || transitionRewards) {
        result.rewardModels.insert(std::make_pair(
            "", storm::models::sparse::StandardRewardModel<RewardValueType>(std::move(stateRewards), std::nullopt, std::move(transitionRewards))));
    }

    // Only parse choice labeling if a file is given.
    std::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;
    if (!choiceLabelingFilename.empty()) {
        result.choiceLabeling = storm::parser::SparseItemLabelingParser::parseChoiceLabeling(result.transitionMatrix.getRowCount(), choiceLabelingFilename);
    }

    return result;
}

template<typename ValueType, typename RewardValueType>
storm::models::sparse::Dtmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
DeterministicModelParser<ValueType, RewardValueType>::parseDtmc(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                std::string const& stateRewardFilename, std::string const& transitionRewardFilename,
                                                                std::string const& choiceLabelingFilename, ExplicitModelParserOptions const& options) {
    auto parserResult =
        parseDeterministicModel(transitionsFilename, labelingFilename, stateRewardFilename, transitionRewardFilename, choiceLabelingFilename, options);
    return storm::models::sparse::Dtmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(std::move(parserResult));
}

template<typename ValueType, typename RewardValueType>
storm::models::sparse::Ctmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>
DeterministicModelParser<ValueType, RewardValueType>::parseCtmc(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                                std::string const& stateRewardFilename, std::string const& transitionRewardFilename,
                                                                std::string const& choiceLabelingFilename, ExplicitModelParserOptions const& options) {
    auto parserResult =
        parseDeterministicModel(transitionsFilename, labelingFilename, stateRewardFilename, transitionRewardFilename, choiceLabelingFilename, options);
    parserResult.rateTransitions = true;
    return storm::models::sparse::Ctmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(std::move(parserResult));
}

template class DeterministicModelParser<double, double>;

template class DeterministicModelParser<double, storm::Interval>;

} /* namespace parser */
} /* namespace storm */
