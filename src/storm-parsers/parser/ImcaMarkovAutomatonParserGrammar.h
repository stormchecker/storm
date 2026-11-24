#pragma once

#include <fstream>
#include <memory>

#include "storm/generator/StateBehavior.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/storage/sparse/ModelComponents.h"

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm-parsers/parser/SpiritErrorHandler.h"

namespace storm {
namespace parser {

template<typename ValueType, typename StateType = uint32_t>
class ImcaParserGrammar : public qi::grammar<Iterator, storm::storage::sparse::ModelComponents<ValueType>(), Skipper> {
   public:
    explicit ImcaParserGrammar(ExplicitModelParserOptions const& options = ExplicitModelParserOptions());

   private:
    void initialize();

    std::pair<StateType, ValueType> createStateValuePair(StateType const& state, ValueType const& value);
    StateType getStateIndex(std::string const& stateString);
    void addInitialState(StateType const& state);
    void addGoalState(StateType const& state);
    void addChoiceToStateBehavior(StateType const& state, std::string const& label, std::vector<std::pair<StateType, ValueType>> const& transitions,
                                  boost::optional<ValueType> const& reward);
    storm::storage::sparse::ModelComponents<ValueType> createModelComponents();

    qi::rule<Iterator, storm::storage::sparse::ModelComponents<ValueType>(), Skipper> start;

    qi::rule<Iterator, qi::unused_type(), Skipper> initials;
    qi::rule<Iterator, qi::unused_type(), Skipper> goals;
    qi::rule<Iterator, qi::unused_type(), Skipper> transitions;

    qi::rule<Iterator, qi::unused_type(), Skipper> choice;
    qi::rule<Iterator, std::pair<StateType, ValueType>(), Skipper> transition;
    qi::rule<Iterator, std::string(), Skipper> choicelabel;
    qi::rule<Iterator, ValueType(), Skipper> reward;
    qi::rule<Iterator, StateType(), Skipper> state;
    qi::rule<Iterator, ValueType(), Skipper> value;

    ExplicitModelParserOptions options;

    StateType numStates;
    StateType numChoices;
    StateType numTransitions;
    bool hasStateReward;
    bool hasActionReward;

    std::vector<storm::generator::StateBehavior<ValueType, StateType>> stateBehaviors;
    storm::storage::BitVector initialStates, goalStates, markovianStates;
    std::map<std::string, StateType> stateIndices;
};

}  // namespace parser
}  // namespace storm
