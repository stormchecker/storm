#pragma once

#include "storm-parsers/parser/MarkovAutomatonSparseTransitionParser.h"
#include "storm/models/sparse/MarkovAutomaton.h"

namespace storm {
namespace parser {

/*!
 * Loads a labeled Markov automaton from files.
 *
 * Given the file paths of the files holding the transitions, the atomic propositions and optionally the state rewards
 * it loads the files, parses them and returns the desired model.
 */
template<typename ValueType = double, typename RewardValueType = double>
class MarkovAutomatonParser {
   public:
    /*!
     * Parses the given Markov automaton and returns an object representing the automaton.
     *
     * @note The number of states of the model is determined by the transitions file.
     *       The labeling file may therefore not contain labels of states that are not contained in the transitions file.
     *
     * @param transitionsFilename The name of the file containing the transitions of the Markov automaton.
     * @param labelingFilename The name of the file containing the labels for the states of the Markov automaton.
     * @param stateRewardFilename The name of the file that contains the state reward of the Markov automaton.
     * @param transitionRewardFilename The name of the file that contains the transition rewards of the Markov automaton. This should be empty as transition
     * rewards are not supported by Markov Automata.
     * @param choiceLabelingFilename The name of the file that contains the choice labels.
     * @return The parsed MarkovAutomaton.
     */
    static storm::models::sparse::MarkovAutomaton<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> parseMarkovAutomaton(
        std::string const& transitionsFilename, std::string const& labelingFilename, std::string const& stateRewardFilename = "",
        std::string const& transitionRewardFilename = "", std::string const& choiceLabelingFilename = "");
};

}  // namespace parser
}  // namespace storm
