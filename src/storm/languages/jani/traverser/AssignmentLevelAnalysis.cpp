#include "storm/languages/jani/traverser/AssignmentLevelAnalysis.h"

#include <tuple>
#include <vector>

#include "storm/languages/jani/Assignment.h"
#include "storm/languages/jani/Automaton.h"
#include "storm/languages/jani/Location.h"
#include "storm/languages/jani/Model.h"
#include "storm/languages/jani/Variable.h"
#include "storm/languages/jani/traverser/JaniTraverser.h"
#include "storm/languages/jani/types/AllJaniTypes.h"

namespace storm {
namespace jani {

int64_t AssignmentLevels::lowestLevel() const {
    return levels.empty() ? 0 : *levels.begin();
}

bool AssignmentLevels::isPassed(storm::expressions::Variable const& variable) const {
    return passedTransientVariables.count(variable) > 0;
}

bool AssignmentLevels::isReadAsSuccessor(storm::expressions::Variable const& variable, int64_t level) const {
    auto it = writeLevel.find(variable);
    return it != writeLevel.end() && it->second < level;
}

namespace {
struct CollectedAssignments {
    // The assignments on edges and their destinations.
    std::vector<Assignment const*> edgeAssignments;
    // The transient values of locations, with the automaton and location they belong to.
    std::vector<std::tuple<Automaton const*, Location const*, Assignment const*>> locationAssignments;
    std::map<storm::expressions::Variable, Variable const*> transientVariables;
};

class AssignmentCollector : public ConstJaniTraverser {
   public:
    CollectedAssignments collect(Model const& model) {
        CollectedAssignments result;
        ConstJaniTraverser::traverse(model, &result);
        return result;
    }

    virtual void traverse(Variable const& variable, boost::any const& data) override {
        if (variable.isTransient()) {
            boost::any_cast<CollectedAssignments*>(data)->transientVariables.emplace(variable.getExpressionVariable(), &variable);
        }
    }

    virtual void traverse(Automaton const& automaton, boost::any const& data) override {
        currentAutomaton = &automaton;
        ConstJaniTraverser::traverse(automaton, data);
    }

    virtual void traverse(Location const& location, boost::any const& data) override {
        for (auto const& assignment : location.getAssignments()) {
            boost::any_cast<CollectedAssignments*>(data)->locationAssignments.emplace_back(currentAutomaton, &location, &assignment);
        }
    }

    virtual void traverse(Assignment const& assignment, boost::any const& data) override {
        boost::any_cast<CollectedAssignments*>(data)->edgeAssignments.push_back(&assignment);
    }

   private:
    Automaton const* currentAutomaton = nullptr;
};
}  // namespace

AssignmentLevels analyzeAssignmentLevels(Model const& model) {
    AssignmentLevels info;
    CollectedAssignments collected = AssignmentCollector().collect(model);

    auto constantsSubstitution = model.getConstantsSubstitution();
    for (auto const& [expressionVariable, variable] : collected.transientVariables) {
        if (variable->hasInitExpression()) {
            info.transientInitialValues.emplace(expressionVariable, variable->getInitExpression().substitute(constantsSubstitution));
        }
    }

    if (model.usesAssignmentLevels() && model.getModelType() == ModelType::MA) {
        info.error = "Markov automata with assignment levels are not supported.";
        return info;
    }

    // Determine the unique write level of every variable.
    for (auto const* assignment : collected.edgeAssignments) {
        info.levels.insert(assignment->getLevel());
        auto it = info.writeLevel.emplace(assignment->getExpressionVariable(), assignment->getLevel()).first;
        if (it->second != assignment->getLevel()) {
            info.error = "Variable '" + assignment->getExpressionVariable().getName() + "' is assigned at levels " +
                         std::to_string(std::min(it->second, assignment->getLevel())) + " and " + std::to_string(std::max(it->second, assignment->getLevel())) +
                         ", but the symbolic engine requires every variable to be assigned at a single level.";
            return info;
        }
    }

    // Determine the passed transient variables.
    auto passedValuesReadBy = [&](Assignment const& assignment) {
        std::set<storm::expressions::Variable> result;
        for (auto const& variable : assignment.getAssignedExpression().getVariables()) {
            if (collected.transientVariables.count(variable) > 0 && info.isReadAsSuccessor(variable, assignment.getLevel())) {
                result.insert(variable);
            }
        }
        return result;
    };
    for (auto const* assignment : collected.edgeAssignments) {
        auto passed = passedValuesReadBy(*assignment);
        info.passedTransientVariables.insert(passed.begin(), passed.end());
    }

    for (auto const& expressionVariable : info.passedTransientVariables) {
        auto const* variable = collected.transientVariables.at(expressionVariable);
        auto const& type = variable->getType();
        bool supportedType = (type.isBasicType() && type.asBasicType().isBooleanType()) || (type.isBoundedType() && type.asBoundedType().isIntegerType());
        if (!supportedType) {
            info.error = "Transient variable '" + variable->getName() +
                         "' is read at a level above the one where it is written, which the symbolic engine only supports for Boolean and bounded integer "
                         "variables.";
            return info;
        }
        if (!variable->hasInitExpression()) {
            info.error = "Transient variable '" + variable->getName() + "' has no initial value.";
            return info;
        }
    }

    // Transient variables that are not passed on (for example reward variables) may not depend on passed values.
    for (auto const* assignment : collected.edgeAssignments) {
        if (!assignment->isTransient() || info.isPassed(assignment->getExpressionVariable())) {
            continue;
        }
        auto passed = passedValuesReadBy(*assignment);
        if (!passed.empty()) {
            info.error = "Transient variable '" + assignment->getExpressionVariable().getName() + "' is assigned a value that depends on '" +
                         passed.begin()->getName() +
                         "', which is written at a lower level within the same step. The symbolic engine does not support this for transient variables "
                         "that are not read at a higher level (such as reward variables).";
            return info;
        }
    }

    // The transient values of locations are evaluated in a state, not in a step, so they may not read passed values.
    for (auto const& [automaton, location, assignment] : collected.locationAssignments) {
        for (auto const& variable : assignment->getAssignedExpression().getVariables()) {
            if (info.isPassed(variable)) {
                info.error = "Transient variable '" + assignment->getExpressionVariable().getName() + "' is assigned a value in location '" +
                             location->getName() + "' of automaton '" + automaton->getName() + "' that depends on '" + variable.getName() +
                             "', which is read at a level above the one where it is written. The symbolic engine does not support this.";
                return info;
            }
        }
    }
    return info;
}

}  // namespace jani
}  // namespace storm
