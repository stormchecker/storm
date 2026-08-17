#include "storm-gamebased-ar/abstraction/MenuGameAbstractor.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/io/file.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/utility/dd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType, typename ValueType>
MenuGameAbstractor<DdType, ValueType>::MenuGameAbstractor()
    : restrictToRelevantStates(storm::settings::getModule<storm::settings::modules::AbstractionSettings>().isRestrictToRelevantStatesSet()) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
std::vector<std::map<storm::expressions::Variable, storm::expressions::Expression>> MenuGameAbstractor<DdType, ValueType>::getVariableUpdates(
    uint64_t player1Choice) const {
    std::vector<std::map<storm::expressions::Variable, storm::expressions::Expression>> result(this->getNumberOfUpdates(player1Choice));
    for (uint64_t i = 0; i < result.size(); ++i) {
        result[i] = this->getVariableUpdates(player1Choice, i);
    }
    return result;
}

template<typename ValueType>
std::string getStateName(std::pair<storm::expressions::SimpleValuation, ValueType> const& stateValue,
                         std::set<storm::expressions::Variable> const& locationVariables, std::set<storm::expressions::Variable> const& predicateVariables,
                         storm::expressions::Variable const& bottomVariable) {
    std::stringstream stateName;

    if (!locationVariables.empty()) {
        stateName << "loc";
    }

    for (auto const& variable : locationVariables) {
        stateName << stateValue.first.getIntegerValue(variable);
    }

    if (!locationVariables.empty() && !predicateVariables.empty()) {
        stateName << "_";
    }

    for (auto const& variable : predicateVariables) {
        if (stateValue.first.getBooleanValue(variable)) {
            stateName << "1";
        } else {
            stateName << "0";
        }
    }

    if (stateValue.first.getBooleanValue(bottomVariable)) {
        stateName << "bot";
    }
    return stateName.str();
}

template<storm::dd::DdType DdType, typename ValueType>
bool MenuGameAbstractor<DdType, ValueType>::isRestrictToRelevantStatesSet() const {
    return restrictToRelevantStates;
}

template<storm::dd::DdType DdType, typename ValueType>
void MenuGameAbstractor<DdType, ValueType>::exportToDot(storm::gbar::abstraction::MenuGame<DdType, ValueType> const& currentGame, std::string const& filename,
                                                        storm::dd::Bdd<DdType> const& highlightStatesBdd, storm::dd::Bdd<DdType> const& filter) const {
    std::ofstream out;
    storm::io::openFile(filename, out);
    AbstractionInformation<DdType> const& abstractionInformation = this->getAbstractionInformation();

    storm::dd::Add<DdType, ValueType> filteredTransitions = filter.template toAdd<ValueType>() * currentGame.getTransitionMatrix();
    storm::dd::Bdd<DdType> filteredTransitionsBdd = filteredTransitions.toBdd().existsAbstract(currentGame.getNondeterminismVariables());
    storm::dd::Bdd<DdType> filteredReachableStates = storm::utility::dd::computeReachableStates(currentGame.getInitialStates(), filteredTransitionsBdd,
                                                                                                currentGame.getRowVariables(), currentGame.getColumnVariables())
                                                         .first;
    filteredTransitions *= filteredReachableStates.template toAdd<ValueType>();

    // Determine all initial states so we can color them blue.
    std::unordered_set<std::string> initialStates;
    storm::dd::Add<DdType, ValueType> initialStatesAsAdd = currentGame.getInitialStates().template toAdd<ValueType>();
    for (auto stateValue : initialStatesAsAdd) {
        initialStates.insert(getStateName(stateValue, abstractionInformation.getSourceLocationVariables(), abstractionInformation.getSourcePredicateVariables(),
                                          abstractionInformation.getBottomStateVariable(true)));
    }

    // Determine all highlight states so we can color them red.
    std::unordered_set<std::string> highlightStates;
    storm::dd::Add<DdType, ValueType> highlightStatesAdd = highlightStatesBdd.template toAdd<ValueType>();
    for (auto stateValue : highlightStatesAdd) {
        highlightStates.insert(getStateName(stateValue, abstractionInformation.getSourceLocationVariables(),
                                            abstractionInformation.getSourcePredicateVariables(), abstractionInformation.getBottomStateVariable(true)));
    }

    out << "digraph game {\n";

    // Create the player 1 nodes.
    storm::dd::Add<DdType, ValueType> statesAsAdd = filteredReachableStates.template toAdd<ValueType>();
    for (auto stateValue : statesAsAdd) {
        out << "\tpl1_";
        std::string stateName = getStateName(stateValue, abstractionInformation.getSourceLocationVariables(),
                                             abstractionInformation.getSourcePredicateVariables(), abstractionInformation.getBottomStateVariable(true));
        out << stateName;
        out << " [ label=\"";
        if (stateValue.first.getBooleanValue(abstractionInformation.getBottomStateVariable(true))) {
            out << "*\", margin=0, width=0, height=0, shape=\"none\"";
        } else {
            out << stateName << "\", margin=0, width=0, height=0, shape=\"oval\"";
        }
        bool isInitial = initialStates.find(stateName) != initialStates.end();
        bool isHighlight = highlightStates.find(stateName) != highlightStates.end();
        if (isInitial && isHighlight) {
            out << ", style=\"filled\", fillcolor=\"yellow\"";
        } else if (isInitial) {
            out << ", style=\"filled\", fillcolor=\"blue\"";
        } else if (isHighlight) {
            out << ", style=\"filled\", fillcolor=\"red\"";
        }
        out << " ];\n";
    }

    // Create the nodes of the second player.
    storm::dd::Add<DdType, ValueType> player2States = filteredTransitions.toBdd()
                                                          .existsAbstract(currentGame.getColumnVariables())
                                                          .existsAbstract(currentGame.getPlayer2Variables())
                                                          .template toAdd<ValueType>();
    for (auto stateValue : player2States) {
        out << "\tpl2_";
        std::string stateName = getStateName(stateValue, abstractionInformation.getSourceLocationVariables(),
                                             abstractionInformation.getSourcePredicateVariables(), abstractionInformation.getBottomStateVariable(true));
        uint_fast64_t index = abstractionInformation.decodePlayer1Choice(stateValue.first, abstractionInformation.getPlayer1VariableCount());
        out << stateName << "_" << index;
        out << " [ shape=\"square\", width=0, height=0, margin=0, label=\"" << index << "\" ];\n";
        out << "\tpl1_" << stateName << " -> " << "pl2_" << stateName << "_" << index << " [ label=\"" << index << "\" ];\n";
    }

    // Create the nodes of the probabilistic player.
    storm::dd::Add<DdType, ValueType> playerPStates = filteredTransitions.toBdd().existsAbstract(currentGame.getColumnVariables()).template toAdd<ValueType>();
    for (auto stateValue : playerPStates) {
        out << "\tplp_";
        std::stringstream stateNameStream;
        stateNameStream << getStateName(stateValue, abstractionInformation.getSourceLocationVariables(), abstractionInformation.getSourcePredicateVariables(),
                                        abstractionInformation.getBottomStateVariable(true));
        uint_fast64_t index = abstractionInformation.decodePlayer1Choice(stateValue.first, abstractionInformation.getPlayer1VariableCount());
        stateNameStream << "_" << index;
        std::string stateName = stateNameStream.str();
        index = abstractionInformation.decodePlayer2Choice(stateValue.first, currentGame.getPlayer2Variables().size());
        out << stateName << "_" << index;
        out << " [ shape=\"point\", label=\"\" ];\n";
        out << "\tpl2_" << stateName << " -> " << "plp_" << stateName << "_" << index << " [ label=\"" << index << "\" ];\n";
    }

    for (auto stateValue : filteredTransitions) {
        std::string sourceStateName = getStateName(stateValue, abstractionInformation.getSourceLocationVariables(),
                                                   abstractionInformation.getSourcePredicateVariables(), abstractionInformation.getBottomStateVariable(true));
        std::string successorStateName =
            getStateName(stateValue, abstractionInformation.getSuccessorLocationVariables(), abstractionInformation.getSuccessorPredicateVariables(),
                         abstractionInformation.getBottomStateVariable(false));
        uint_fast64_t pl1Index = abstractionInformation.decodePlayer1Choice(stateValue.first, abstractionInformation.getPlayer1VariableCount());
        uint_fast64_t pl2Index = abstractionInformation.decodePlayer2Choice(stateValue.first, currentGame.getPlayer2Variables().size());
        out << "\tplp_" << sourceStateName << "_" << pl1Index << "_" << pl2Index << " -> pl1_" << successorStateName << " [ label=\"" << stateValue.second
            << "\"];\n";
    }

    out << "}\n";
    storm::io::closeFile(out);
}

template<storm::dd::DdType DdType, typename ValueType>
void MenuGameAbstractor<DdType, ValueType>::setTargetStates(storm::expressions::Expression const& targetStateExpression) {
    this->targetStateExpression = targetStateExpression;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Expression const& MenuGameAbstractor<DdType, ValueType>::getTargetStateExpression() const {
    return this->targetStateExpression;
}

template<storm::dd::DdType DdType, typename ValueType>
bool MenuGameAbstractor<DdType, ValueType>::hasTargetStateExpression() const {
    return this->targetStateExpression.isInitialized();
}

template class MenuGameAbstractor<storm::dd::DdType::CUDD, double>;
template class MenuGameAbstractor<storm::dd::DdType::Sylvan, double>;
template class MenuGameAbstractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace abstraction
}  // namespace storm::gbar
