#include "storm-parsers/parser/ImcaMarkovAutomatonParserGrammar.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/io/file.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/utility/builder.h"

namespace storm {
namespace parser {

template<typename ValueType, typename StateType>
ImcaParserGrammar<ValueType, StateType>::ImcaParserGrammar()
    : ImcaParserGrammar<ValueType, StateType>::base_type(start), numStates(0), numChoices(0), numTransitions(0), hasStateReward(false), hasActionReward(false) {
    buildChoiceLabels = storm::settings::getModule<storm::settings::modules::BuildSettings>().isBuildChoiceLabelsSet();
    initialize();
}

template<typename ValueType, typename StateType>
void ImcaParserGrammar<ValueType, StateType>::initialize() {
    value = qi::double_[qi::_val = qi::_1];
    value.name("value");

    // We assume here that imca files are alphanumeric strings, If we restrict ourselves to the 's12345' representation, we could also do:
    //       state = (qi::lit("s") > qi::ulong_)[qi::_val = qi::_1];
    state = qi::as_string[qi::raw[qi::lexeme[(qi::alnum | qi::char_('_')) % qi::eps]]]
                         [qi::_val = phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::getStateIndex, phoenix::ref(*this), qi::_1)];
    state.name("state");

    reward = (-qi::lit("R") >> value)[qi::_val = qi::_1];
    reward.name("reward");

    transition = (qi::lit("*") >> state >>
                  value)[qi::_val = phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::createStateValuePair, phoenix::ref(*this), qi::_1, qi::_2)];
    transition.name("transition");

    choicelabel = qi::as_string[qi::raw[qi::lexeme[((qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_')) | qi::lit("!"))]]];
    choicelabel.name("choicelabel");

    choice =
        (state >> choicelabel >> -reward >>
         *(transition >>
           qi::eps))[phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::addChoiceToStateBehavior, phoenix::ref(*this), qi::_1, qi::_2, qi::_4, qi::_3)];
    choice.name("choice");

    transitions = qi::lit("#TRANSITIONS") >> *(choice);
    transitions.name("TRANSITIONS");

    initials =
        qi::lit("#INITIALS") >> *((state >> qi::eps)[phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::addInitialState, phoenix::ref(*this), qi::_1)]);
    initials.name("INITIALS");

    goals = qi::lit("#GOALS") >> *((state >> qi::eps)[phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::addGoalState, phoenix::ref(*this), qi::_1)]);
    goals.name("GOALS");

    start = (initials >> goals >> transitions)[qi::_val = phoenix::bind(&ImcaParserGrammar<ValueType, StateType>::createModelComponents, phoenix::ref(*this))];
    start.name("start");
}

template<typename ValueType, typename StateType>
StateType ImcaParserGrammar<ValueType, StateType>::getStateIndex(std::string const& stateString) {
    auto it = stateIndices.find(stateString);
    if (it == stateIndices.end()) {
        this->stateIndices.emplace_hint(it, stateString, numStates);
        ++numStates;
        initialStates.grow(numStates);
        goalStates.grow(numStates);
        markovianStates.grow(numStates);
        stateBehaviors.resize(numStates);
        return numStates - 1;
    } else {
        return it->second;
    }
}

template<typename ValueType, typename StateType>
std::pair<StateType, ValueType> ImcaParserGrammar<ValueType, StateType>::createStateValuePair(StateType const& state, ValueType const& value) {
    return std::pair<StateType, ValueType>(state, value);
}

template<typename ValueType, typename StateType>
void ImcaParserGrammar<ValueType, StateType>::addInitialState(StateType const& state) {
    initialStates.set(state);
}

template<typename ValueType, typename StateType>
void ImcaParserGrammar<ValueType, StateType>::addGoalState(StateType const& state) {
    goalStates.set(state);
}

template<typename ValueType, typename StateType>
void ImcaParserGrammar<ValueType, StateType>::addChoiceToStateBehavior(StateType const& state, std::string const& label,
                                                                       std::vector<std::pair<StateType, ValueType>> const& transitions,
                                                                       boost::optional<ValueType> const& reward) {
    bool isMarkovian = label == "!";
    storm::generator::Choice<ValueType, StateType> choice(0, isMarkovian);
    STORM_LOG_THROW(!transitions.empty(), storm::exceptions::WrongFormatException, "Empty choice defined for state s" << state << ".");
    if (buildChoiceLabels && !isMarkovian) {
        choice.addLabel(label);
    }
    if (reward && !isMarkovian) {
        hasActionReward = true;
        choice.addReward(reward.get());
    }
    for (auto const& t : transitions) {
        STORM_LOG_THROW(t.second > storm::utility::zero<ValueType>(), storm::exceptions::WrongFormatException,
                        "Probabilities and rates have to be positive. got " << t.second << " at state s" << state << ".");
        choice.addProbability(t.first, t.second);
    }
    STORM_LOG_THROW(isMarkovian || storm::utility::isOne(choice.getTotalMass()), storm::exceptions::WrongFormatException,
                    "Probability for choice " << label << " on state s" << state << " does not sum up to one.");

    ++numChoices;
    numTransitions += choice.size();
    auto& behavior = stateBehaviors[state];
    behavior.setExpanded(true);
    behavior.addChoice(std::move(choice));
    if (isMarkovian) {
        markovianStates.set(state);
        if (reward) {
            hasStateReward = true;
            behavior.addStateReward(reward.get());
        }
    }
}

template<typename ValueType, typename StateType>
storm::storage::sparse::ModelComponents<ValueType> ImcaParserGrammar<ValueType, StateType>::createModelComponents() {
    // Prepare the statelabeling
    initialStates.resize(numStates);
    goalStates.resize(numStates);
    markovianStates.resize(numStates);
    storm::models::sparse::StateLabeling stateLabeling(numStates);
    stateLabeling.addLabel("init", std::move(initialStates));
    stateLabeling.addLabel("goal", std::move(goalStates));

    // Fix deadlocks (if required)
    STORM_LOG_ASSERT(stateBehaviors.size() == numStates, "State behavior count mismatch.");
    if (!storm::settings::getModule<storm::settings::modules::BuildSettings>().isDontFixDeadlocksSet()) {
        StateType state = 0;
        for (auto& behavior : stateBehaviors) {
            if (!behavior.wasExpanded()) {
                storm::generator::Choice<ValueType, StateType> choice(0, true);
                choice.addProbability(state, storm::utility::one<ValueType>());
                behavior.setExpanded(true);
                behavior.addChoice(std::move(choice));
                markovianStates.set(state);
                ++numChoices;
                ++numTransitions;
            }
            ++state;
        }
    }

    // Build the transition matrix together with exit rates, reward models, and choice labeling
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(numChoices, numStates, numTransitions, true, true, numStates);
    std::vector<ValueType> exitRates;
    exitRates.reserve(numStates);
    std::optional<std::vector<ValueType>> stateRewards, actionRewards;
    if (hasStateReward) {
        stateRewards = std::vector<ValueType>(numStates, storm::utility::zero<ValueType>());
    }
    if (hasActionReward) {
        actionRewards = std::vector<ValueType>(numChoices, storm::utility::zero<ValueType>());
    }
    std::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;
    if (buildChoiceLabels) {
        choiceLabeling = storm::models::sparse::ChoiceLabeling(numChoices);
    }
    StateType state = 0;
    StateType row = 0;
    for (auto const& behavior : stateBehaviors) {
        matrixBuilder.newRowGroup(row);
        if (!behavior.getStateRewards().empty()) {
            STORM_LOG_ASSERT(behavior.getStateRewards().size() == 1, "Expected single state reward.");
            stateRewards.value()[state] = behavior.getStateRewards().front();
        }
        if (markovianStates.get(state)) {
            // For Markovian states, the Markovian choice has to be the first one in the resulting transition matrix.
            bool markovianChoiceFound = false;
            for (auto const& choice : behavior) {
                if (choice.isMarkovian()) {
                    STORM_LOG_THROW(!markovianChoiceFound, storm::exceptions::WrongFormatException,
                                    "Multiple Markovian choices defined for state " << state << ".");
                    markovianChoiceFound = true;
                    if (!choice.getRewards().empty()) {
                        STORM_LOG_ASSERT(choice.getRewards().size() == 1, "Expected exactly one reward per choice.");
                        actionRewards.value()[row] = choice.getRewards().front();
                    }
                    if (buildChoiceLabels && choice.hasLabels()) {
                        STORM_LOG_ASSERT(choice.getLabels().size() == 1, "Expected exactly one label per choice.");
                        std::string const& label = *choice.getLabels().begin();
                        if (!choiceLabeling->containsLabel(label)) {
                            choiceLabeling->addLabel(label);
                        }
                        choiceLabeling->addLabelToChoice(label, row);
                    }
                    exitRates.push_back(choice.getTotalMass());
                    for (auto const& transition : choice) {
                        matrixBuilder.addNextValue(row, transition.first, static_cast<ValueType>(transition.second / exitRates.back()));
                    }
                    ++row;
                }
            }
        } else {
            exitRates.push_back(storm::utility::zero<ValueType>());
        }
        // Now add all probabilistic choices.
        for (auto const& choice : behavior) {
            if (!choice.isMarkovian()) {
                if (!choice.getRewards().empty()) {
                    STORM_LOG_ASSERT(choice.getRewards().size() == 1, "Expected exactly one reward per choice.");
                    actionRewards.value()[row] = choice.getRewards().front();
                }
                if (buildChoiceLabels && choice.hasLabels()) {
                    STORM_LOG_ASSERT(choice.getLabels().size() == 1, "Expected exactly one label per choice.");
                    std::string const& label = *choice.getLabels().begin();
                    if (!choiceLabeling->containsLabel(label)) {
                        choiceLabeling->addLabel(label);
                    }
                    choiceLabeling->addLabelToChoice(label, row);
                }
                for (auto const& transition : choice) {
                    matrixBuilder.addNextValue(row, transition.first, transition.second);
                }
                ++row;
            }
        }
        ++state;
    }

    // Finalize the model components
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModel;
    if (hasStateReward || hasActionReward) {
        rewardModel.insert(std::make_pair("", storm::models::sparse::StandardRewardModel<ValueType>(stateRewards, actionRewards)));
    }
    storm::storage::sparse::ModelComponents<ValueType> components(matrixBuilder.build(), std::move(stateLabeling), std::move(rewardModel), false,
                                                                  std::move(markovianStates));
    components.exitRates = std::move(exitRates);
    components.choiceLabeling = std::move(choiceLabeling);

    return components;
}

template class ImcaParserGrammar<double>;
}  // namespace parser
}  // namespace storm
