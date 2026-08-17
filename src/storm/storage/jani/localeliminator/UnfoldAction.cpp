#include "UnfoldAction.h"
#include <boost/format.hpp>
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/JaniLocationExpander.h"

namespace storm {
namespace jani {
namespace elimination_actions {
UnfoldAction::UnfoldAction(const std::string &automatonName, const std::string &variableName) {
    this->automatonName = automatonName;
    this->variableName = variableName;
    this->expressionVariableName = variableName;
}
UnfoldAction::UnfoldAction(const std::string &automatonName, const std::string &janiVariableName, const std::string &expressionVariableName) {
    this->automatonName = automatonName;
    this->variableName = janiVariableName;
    this->expressionVariableName = expressionVariableName;
}

std::string UnfoldAction::getDescription() {
    return (boost::format("UnfoldAction (Automaton %s, Variable %s)") % automatonName % variableName).str();
}

void UnfoldAction::doAction(JaniLocalEliminator::Session &session) {
    // In addition to doing the unfolding, we also need to update which location might satisfy the property.
    // If a location that doesn't satisfy the property is unfolded, all resulting locations also won't
    // satisfy it. We therefore first store which old locations satisfy the property:

    STORM_LOG_THROW(session.getModel().hasAutomaton(automatonName), storm::exceptions::IllegalArgumentException,
                    "Cannot find automaton with name " << automatonName << ".");

    uint64_t partOfPropCount = 0;

    std::map<uint64_t, bool> partOfProp;
    auto automaton = session.getModel().getAutomaton(automatonName);
    for (uint64_t i = 0; i < automaton.getNumberOfLocations(); i++) {
        partOfProp[i] = session.isPartOfProp(automatonName, i);
        if (partOfProp[i])
            partOfPropCount += 1;
    }

    STORM_LOG_TRACE("\t\t" + std::to_string(partOfPropCount) + " old locations potentially satisfy property");

    auto &automatonInfo = session.getAutomatonInfo(automatonName);

    JaniLocationExpander expander = JaniLocationExpander(session.getModel());

    if (automatonInfo.hasSink) {
        expander.excludeLocation(automatonInfo.sinkIndex);
    }

    auto result = expander.transform(automatonName, variableName);
    session.setModel(result.newModel);

    if (automatonInfo.hasSink) {
        automatonInfo.sinkIndex = result.newIndices.excludedLocationsToNewIndices[automatonInfo.sinkIndex];
    }

    // After executing the expander, we can now determine which new locations satisfy the property:

    // First, we check whether the variable is even contained in the property. If not, we can just use the
    // isPartOfProp values of the old locations.
    bool variablePartOfProperty = session.isVariablePartOfProperty(expressionVariableName);

    // If we have many locations that potentially satisfy the property, we can try to exploit symmetries. For
    // example, if the property is a conjunction that contains "testVar = 4" and we're unfolding testVar, we
    // don't have to check whether testVar = 0 satisfies the property for all locations -- if sufficies to
    // check once, globally. We store values for which the property is never satisfied in knownUnsatValues:
    std::set<uint64_t> knownUnsatValues;
    // If we only have 1 or 2 locations, the overhead of checking globally probably isn't worth it, so only
    // check if we have at least 3 locations that potentially satisfy the property.
    if (partOfPropCount >= 3) {
        std::map<expressions::Variable, expressions::Expression> substitutionMap;
        expressions::Variable variable = session.getModel().getExpressionManager().getVariable(expressionVariableName);
        for (uint64_t i = 0; i < result.newIndices.variableDomain.size(); i++) {
            substitutionMap[variable] = result.newIndices.variableDomain[i];
            bool satisfiesProperty = session.computeIsPartOfProp(substitutionMap);
            if (!satisfiesProperty) {
                knownUnsatValues.emplace(i);
            }
        }
        STORM_LOG_TRACE("\t\t" + std::to_string(knownUnsatValues.size()) + " variable values never satisfy property");
    }

    // If true, this doesn't perform satisfiability checks and instead simply assumes that any location
    // potentially satisfies the property unless it can be disproven.
    bool avoidChecks = false;
    if (partOfPropCount > 5 && automaton.getNumberOfLocations() > partOfPropCount * 5) {
        avoidChecks = true;
    }

    // These are just used for statistics:
    uint64_t knownUnsatCounter = 0;
    uint64_t satisfactionCheckCounter = 0;
    uint64_t knownSatCounter = 0;
    uint64_t oldLocationUnsatCounter = 0;

    for (std::pair<uint64_t, std::map<int64_t, uint64_t>> oldLocMapping : result.newIndices.locationVariableValueMap) {
        bool oldSatisfied = partOfProp[oldLocMapping.first];
        for (std::pair<uint64_t, uint64_t> valueIndexPair : oldLocMapping.second) {
            if (oldSatisfied) {
                bool isPartOfProp;
                if (variablePartOfProperty) {
                    if (knownUnsatValues.count(valueIndexPair.first) > 0) {
                        isPartOfProp = false;
                        knownUnsatCounter++;
                    } else {
                        if (avoidChecks) {
                            isPartOfProp = true;
                        } else {
                            isPartOfProp = session.computeIsPartOfProp(automatonName, valueIndexPair.second);
                        }
                        satisfactionCheckCounter++;
                    }
                } else {
                    // If the variable isn't contained in the property, this location will satisfy the
                    // property, because the old one also did.
                    isPartOfProp = true;
                    knownSatCounter++;
                }
                session.setPartOfProp(automatonName, valueIndexPair.second, isPartOfProp);
            } else {
                session.setPartOfProp(automatonName, valueIndexPair.second, false);
                oldLocationUnsatCounter++;
            }
        }
    }

    uint64_t totalCount = knownUnsatCounter + satisfactionCheckCounter + knownSatCounter + oldLocationUnsatCounter;
    STORM_LOG_TRACE("\t\tPerformed " + std::to_string(satisfactionCheckCounter) + " property satisfaction checks (location count: " +
                    std::to_string(totalCount) + "), avoided\n\t\t\t" + std::to_string(oldLocationUnsatCounter) + " because old location was unsat,\n\t\t\t" +
                    std::to_string(knownSatCounter) + " because variable was not part of property and old location was sat and\n\t\t\t" +
                    std::to_string(knownUnsatCounter) + " because variable value was known to never satisfy property.");
}
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
