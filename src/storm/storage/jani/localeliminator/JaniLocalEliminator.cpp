#include "storm/storage/jani/localeliminator/JaniLocalEliminator.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/solver/Z3SmtSolver.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/localeliminator/AutomaticAction.h"
#include "storm/storage/jani/localeliminator/FinishAction.h"

#include "storm/exceptions/NotImplementedException.h"

namespace storm {
namespace jani {

JaniLocalEliminator::JaniLocalEliminator(Model const &original, storm::jani::Property &property, bool addMissingGuards)
    : original(original), addMissingGuards(addMissingGuards) {
    setProperty(property);
    scheduler = EliminationScheduler();
}

JaniLocalEliminator::JaniLocalEliminator(const Model &original, std::vector<storm::jani::Property> &properties, bool addMissingGuards)
    : original(original), addMissingGuards(addMissingGuards) {
    if (properties.size() > 1) {
        STORM_LOG_WARN("Only the first property will be used for local elimination.");
    }
    STORM_LOG_THROW(!properties.empty(), storm::exceptions::InvalidArgumentException, "Local elimination requires at least one property.");

    setProperty(properties[0]);

    scheduler = EliminationScheduler();
}

Model JaniLocalEliminator::eliminateAutomatically(const Model &model, std::vector<jani::Property> properties, uint64_t locationHeuristic,
                                                  uint64_t edgesHeuristic) {
    auto eliminator = storm::jani::JaniLocalEliminator(model, properties);
    eliminator.scheduler.addAction(std::make_unique<elimination_actions::AutomaticAction>(locationHeuristic, edgesHeuristic));
    eliminator.eliminate();
    return eliminator.getResult();
}

void JaniLocalEliminator::eliminate(bool flatten) {
    newModel = original;

    Session session = Session(newModel, property, flatten);

    if (addMissingGuards) {
        session.addMissingGuards(session.getModel().getAutomaton(0).getName());
    }

    for (auto &automaton : session.getModel().getAutomata()) {
        bool hasTransientAssignments = false;
        for (auto loc : automaton.getLocations()) {
            if (loc.getAssignments().hasTransientAssignment()) {
                hasTransientAssignments = true;
            }
        }
        if (hasTransientAssignments) {
            STORM_LOG_TRACE("Pushing transient location assignments to edge destinations");
            automaton.pushTransientRealLocationAssignmentsToEdges();
            automaton.pushEdgeAssignmentsToDestinations();
        }
    }

    while (!session.getFinished()) {
        std::unique_ptr<Action> action = scheduler.getNextAction();
        action->doAction(session);
    }

    newModel = session.getModel();
    newModel.finalize();
}

Model const &JaniLocalEliminator::getResult() {
    return newModel;
}

bool JaniLocalEliminator::Session::isEliminable(const std::string &automatonName, std::string const &locationName) {
    return !isPossiblyInitial(automatonName, locationName) && !hasLoops(automatonName, locationName) && !isPartOfProp(automatonName, locationName);
}
bool JaniLocalEliminator::Session::hasLoops(const std::string &automatonName, std::string const &locationName) {
    Automaton &automaton = model.getAutomaton(automatonName);
    uint64_t locationIndex = automaton.getLocationIndex(locationName);
    for (Edge edge : automaton.getEdgesFromLocation(locationIndex)) {
        for (const EdgeDestination &dest : edge.getDestinations()) {
            if (dest.getLocationIndex() == locationIndex)
                return true;
        }
    }
    return false;
}
bool JaniLocalEliminator::Session::hasNamedActions(const std::string &automatonName, std::string const &locationName) {
    Automaton &automaton = model.getAutomaton(automatonName);
    uint64_t locationIndex = automaton.getLocationIndex(locationName);
    for (const Edge &edge : automaton.getEdgesFromLocation(locationIndex)) {
        if (!edge.hasSilentAction()) {
            return true;
        }
    }
    return false;
}

bool JaniLocalEliminator::Session::isPossiblyInitial(const std::string &automatonName, std::string const &locationName) {
    Automaton &automaton = model.getAutomaton(automatonName);
    auto location = automaton.getLocation(automaton.getLocationIndex(locationName));
    for (const auto &asg : location.getAssignments()) {
        if (!asg.isTransient())
            continue;
        if (asg.getAssignedExpression().containsVariables() ||
            (asg.getVariable().hasInitExpression() && asg.getVariable().getInitExpression().containsVariables()))
            continue;
        if (asg.getVariable().getType().isBoundedType() && asg.getVariable().getType().asBoundedType().isIntegerType()) {
            if (asg.getVariable().hasInitExpression()) {
                int initValue = asg.getVariable().getInitExpression().evaluateAsInt();
                int currentValue = asg.getAssignedExpression().evaluateAsInt();
                if (initValue != currentValue)
                    return false;
            } else {
                STORM_LOG_WARN("Variable " + asg.getVariable().getName() + " has no init expression. The result may not be correct.");
            }
        } else if (asg.getVariable().getType().isBasicType() && asg.getVariable().getType().asBasicType().isBooleanType()) {
            if (asg.getVariable().hasInitExpression()) {
                bool initValue = asg.getVariable().getInitExpression().evaluateAsBool();
                bool currentValue = asg.getAssignedExpression().evaluateAsBool();
                if (initValue != currentValue)
                    return false;
            } else {
                STORM_LOG_WARN("Variable " + asg.getVariable().getName() + " has no init expression. The result may not be correct.");
            }
        }
    }

    return true;
}

bool JaniLocalEliminator::Session::isPartOfProp(const std::string &automatonName, std::string const &locationName) {
    uint64_t locationIndex = model.getAutomaton(automatonName).getLocationIndex(locationName);
    return isPartOfProp(automatonName, locationIndex);
}

bool JaniLocalEliminator::Session::isPartOfProp(const std::string &automatonName, uint64_t locationIndex) {
    AutomatonInfo &autInfo = automataInfo[automatonName];
    return autInfo.potentiallyPartOfProp.count(locationIndex) == 1;
}

bool JaniLocalEliminator::Session::computeIsPartOfProp(const std::string &automatonName, const std::string &locationName) {
    Automaton &automaton = model.getAutomaton(automatonName);
    uint64_t locationIndex = automaton.getLocationIndex(locationName);
    return computeIsPartOfProp(automatonName, locationIndex);
}

bool JaniLocalEliminator::Session::computeIsPartOfProp(const std::string &automatonName, uint64_t locationIndex) {
    Automaton &automaton = model.getAutomaton(automatonName);
    auto location = automaton.getLocation(locationIndex);
    std::map<expressions::Variable, expressions::Expression> substitutionMap;
    for (auto &asg : location.getAssignments()) {
        if (!asg.isTransient())
            continue;
        substitutionMap.insert(std::pair<expressions::Variable, expressions::Expression>(asg.getExpressionVariable(), asg.getAssignedExpression()));
    }
    return computeIsPartOfProp(substitutionMap);
}

bool JaniLocalEliminator::Session::computeIsPartOfProp(const std::map<expressions::Variable, expressions::Expression> &substitutionMap) {
    storm::solver::Z3SmtSolver solver(model.getExpressionManager());
    auto propertyFormula = property.getRawFormula()->substitute(substitutionMap);
    auto expression = model.getExpressionManager().boolean(false);
    if (propertyFormula->isProbabilityOperatorFormula() || propertyFormula->isRewardOperatorFormula()) {
        auto subformula = &propertyFormula->asUnaryStateFormula().getSubformula();
        if (subformula->isEventuallyFormula()) {
            expression = subformula->asEventuallyFormula().getSubformula().toExpression(model.getExpressionManager());
        } else if (subformula->isUntilFormula()) {
            const auto &untilFormula = subformula->asUntilFormula();
            if (untilFormula.getLeftSubformula().isTrueFormula() && untilFormula.getRightSubformula().isAtomicExpressionFormula()) {
                expression = untilFormula.getRightSubformula().toExpression(model.getExpressionManager());
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Until formulas are only supported if the left subformula is \"true\".");
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This type of formula is not supported.");
        }
    }
    auto simplified = expression.simplify();
    if (simplified.isLiteral()) {
        return simplified.evaluateAsBool();
    }
    solver.add(simplified);

    auto result = solver.check();
    return result != storm::solver::SmtSolver::CheckResult::Unsat;
}

void JaniLocalEliminator::Session::setPartOfProp(const std::string &automatonName, const std::string &locationName, bool isPartOfProp) {
    uint64_t locationIndex = model.getAutomaton(automatonName).getLocationIndex(locationName);
    return setPartOfProp(automatonName, locationIndex, isPartOfProp);
}

void JaniLocalEliminator::Session::setPartOfProp(const std::string &automatonName, uint64_t locationIndex, bool isPartOfProp) {
    AutomatonInfo &autInfo = automataInfo[automatonName];
    if (autInfo.potentiallyPartOfProp.count(locationIndex) == 1) {
        if (!isPartOfProp) {
            autInfo.potentiallyPartOfProp.erase(locationIndex);
        }
    } else {
        if (isPartOfProp) {
            autInfo.potentiallyPartOfProp.insert(locationIndex);
        }
    }
}

void JaniLocalEliminator::Session::clearIsPartOfProp(const std::string &automatonName) {
    AutomatonInfo &autInfo = automataInfo[automatonName];
    autInfo.potentiallyPartOfProp.clear();
}

void JaniLocalEliminator::setProperty(storm::jani::Property &newProperty) {
    auto raw = newProperty.getRawFormula();
    bool supported = false;

    if (raw->isProbabilityOperatorFormula()) {
        auto subformula = &raw->asProbabilityOperatorFormula().getSubformula();
        if (subformula->isEventuallyFormula()) {
            supported = true;
        } else if (subformula->isUntilFormula()) {
            const auto &untilFormula = subformula->asUntilFormula();
            if (untilFormula.getLeftSubformula().isTrueFormula()) {
                supported = true;
            }
        }
    }
    if (raw->isRewardOperatorFormula()) {
        auto subformula = &raw->asRewardOperatorFormula().getSubformula();
        if (subformula->isEventuallyFormula()) {
            supported = true;
        } else if (subformula->isUntilFormula()) {
            const auto &untilFormula = subformula->asUntilFormula();
            if (untilFormula.getLeftSubformula().isTrueFormula()) {
                supported = true;
            }
        }
    }

    STORM_LOG_THROW(supported, storm::exceptions::NotSupportedException, "This type of property is not supported for location elimination.");

    this->property = newProperty;
}

JaniLocalEliminator::EliminationScheduler::EliminationScheduler() {}

std::unique_ptr<JaniLocalEliminator::Action> JaniLocalEliminator::EliminationScheduler::getNextAction() {
    if (actionQueue.empty()) {
        return std::make_unique<elimination_actions::FinishAction>();
    }
    std::unique_ptr<JaniLocalEliminator::Action> val = std::move(actionQueue.front());
    actionQueue.pop();
    return val;
}

void JaniLocalEliminator::EliminationScheduler::addAction(std::unique_ptr<JaniLocalEliminator::Action> action) {
    actionQueue.push(std::move(action));
}

JaniLocalEliminator::Session::Session(Model model, Property property, bool flatten) : model(model), property(property), finished(false) {
    if (flatten && model.getNumberOfAutomata() > 1) {
        flatten_automata();
    }

    buildAutomataInfo();

    if (property.getRawFormula()->isRewardOperatorFormula()) {
        isRewardFormula = true;
        rewardModels = property.getRawFormula()->getReferencedRewardModels();
    } else if (property.getRawFormula()->isProbabilityOperatorFormula()) {
        isRewardFormula = false;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This type of property is currently not supported.");
    }

    for (auto &var : property.getUsedVariablesAndConstants()) {
        expressionVarsInProperty.insert(var.getIndex());
    }
}

Model &JaniLocalEliminator::Session::getModel() {
    return model;
}

void JaniLocalEliminator::Session::setModel(const Model &model) {
    this->model = model;
}

Property &JaniLocalEliminator::Session::getProperty() {
    return property;
}

bool JaniLocalEliminator::Session::getFinished() const {
    return finished;
}

void JaniLocalEliminator::Session::setFinished(bool finished) {
    this->finished = finished;
}

expressions::Expression JaniLocalEliminator::Session::getNewGuard(const Edge &edge, const EdgeDestination &dest, const Edge &outgoing) {
    expressions::Expression wp = outgoing.getGuard().substitute(dest.getAsVariableToExpressionMap()).simplify();
    return (edge.getGuard() && wp).simplify();
}

expressions::Expression JaniLocalEliminator::Session::getProbability(const EdgeDestination &first, const EdgeDestination &then) {
    return (first.getProbability() * then.getProbability().substitute(first.getAsVariableToExpressionMap())).simplify();
}

OrderedAssignments JaniLocalEliminator::Session::executeInSequence(const EdgeDestination &first, const EdgeDestination &then,
                                                                   std::set<std::string> &rewardVariables) {
    STORM_LOG_THROW(!first.usesAssignmentLevels() && !then.usesAssignmentLevels(), storm::exceptions::NotImplementedException,
                    "Assignment levels are currently not supported.");

    OrderedAssignments newOa;

    // This method takes two OrderedAssignments and returns an OrderedAssignments that is equivalent to executing the two in sequence.
    // This is done by removing those assignments from the first set that also occur in the second set (because that first assignment would be
    // overwritten by the second one). We then modify the second assignment to that variable so that we still get the same end result.

    // Collect variables that occur in the second set of assignments. This will be used to decide which first assignments to keep and which to discard.
    std::set<expressions::Variable> thenVariables;
    for (const auto &assignment : then.getOrderedAssignments()) {
        thenVariables.emplace(assignment.getExpressionVariable());
    }

    // Add the remaining assignments from the first OrderedAssignments to the new assignment.
    for (const auto &assignment : first.getOrderedAssignments()) {
        if (thenVariables.find(assignment.getExpressionVariable()) != thenVariables.end()) {
            continue;
        }
        newOa.add(assignment);
    }

    // Finally add all assignments from the second OrderedAssignments to the new OrderedAssignments. While doing this, we need to replace all variables
    // that were updated in the first assignment with the expression assigned to them.
    std::map<expressions::Variable, expressions::Expression> substitutionMap = first.getAsVariableToExpressionMap();
    for (const auto &assignment : then.getOrderedAssignments()) {
        bool isReward = rewardVariables.count(assignment.getExpressionVariable().getName());
        auto firstAssignment = substitutionMap.find(assignment.getExpressionVariable());
        if (isReward && firstAssignment != substitutionMap.end()) {
            auto newAssignment = firstAssignment->second + assignment.getAssignedExpression().substitute(substitutionMap);

            newOa.add(Assignment(assignment.getVariable(), newAssignment));
        } else {
            newOa.add(Assignment(assignment.getVariable(), assignment.getAssignedExpression().substitute(substitutionMap).simplify()));
        }
    }
    return newOa;
}

bool JaniLocalEliminator::Session::isVariablePartOfProperty(const std::string &expressionVariableName) {
    auto expressionVariable = model.getExpressionManager().getVariable(expressionVariableName);
    uint_fast64_t expressionVariableIndex = expressionVariable.getIndex();
    return expressionVarsInProperty.count(expressionVariableIndex) != 0;
}

void JaniLocalEliminator::Session::flatten_automata() {
    model = model.flattenComposition();
    automataInfo.clear();
    buildAutomataInfo();
}

void JaniLocalEliminator::Session::addMissingGuards(const std::string &automatonName) {
    auto &automaton = model.getAutomaton(automatonName);

    std::string sinkName = "sink_location";
    while (automaton.hasLocation(sinkName)) {
        sinkName += "_";
    }
    Location sink(sinkName, OrderedAssignments());
    automaton.addLocation(sink);
    uint64_t sinkIndex = automaton.getNumberOfLocations() - 1;

    automataInfo[automatonName].hasSink = true;
    automataInfo[automatonName].sinkIndex = sinkIndex;

    for (uint64_t i = 0; i < automaton.getNumberOfLocations(); i++) {
        if (i == sinkIndex)
            continue;
        auto outgoingEdges = automaton.getEdgesFromLocation(i);
        expressions::Expression allGuards;
        allGuards = model.getExpressionManager().boolean(false);
        for (const auto &edge : outgoingEdges) {
            allGuards = edge.getGuard() || allGuards;
        }
        expressions::Expression newGuard = !allGuards;

        // Before we add the edge, check whether it is satisfiable:
        auto variables = newGuard.getVariables();
        storm::solver::Z3SmtSolver solver(model.getExpressionManager());
        solver.add(newGuard);
        for (const auto &var : model.getGlobalVariables()) {
            if (var.getType().isBoundedType() && var.getType().asBoundedType().isIntegerType() && variables.count(var.getExpressionVariable()) > 0) {
                auto &biVariable = var.getType().asBoundedType();
                solver.add(var.getExpressionVariable().getExpression() >= biVariable.getLowerBound());
                solver.add(var.getExpressionVariable().getExpression() <= biVariable.getUpperBound());
            }
        }
        for (const auto &var : automaton.getVariables()) {
            if (var.getType().isBoundedType() && var.getType().asBoundedType().isIntegerType() && variables.count(var.getExpressionVariable()) > 0) {
                auto &biVariable = var.getType().asBoundedType();
                solver.add(var.getExpressionVariable().getExpression() >= biVariable.getLowerBound());
                solver.add(var.getExpressionVariable().getExpression() <= biVariable.getUpperBound());
            }
        }
        auto result = solver.check();

        if (result != storm::solver::SmtSolver::CheckResult::Unsat) {
            STORM_LOG_TRACE("\tAdding missing guard from location " + automaton.getLocation(i).getName());
            if (result == storm::solver::SmtSolver::CheckResult::Sat) {
                STORM_LOG_TRACE("\t\tThe guard was satisfiable with assignment\n"
                                << ([&] {
                                       auto satisfyingAssignment = solver.getModel();
                                       std::string message;
                                       for (auto &var : variables) {
                                           if (var.hasIntegerType()) {
                                               message += "\t\t\t" + var.getName() + ": " + std::to_string(satisfyingAssignment->getIntegerValue(var));
                                           } else if (var.hasBooleanType()) {
                                               message += "\t\t\t" + var.getName() + ": " + std::to_string(satisfyingAssignment->getBooleanValue(var));
                                           } else if (var.hasRationalType()) {
                                               message += "\t\t\t" + var.getName() + ": " + std::to_string(satisfyingAssignment->getRationalValue(var));
                                           }
                                       }
                                       return message;
                                   })());
            } else {
                STORM_LOG_TRACE("\t\tThe solver could not determine whether the guard was satisfiable");
            }
            std::vector<std::pair<uint64_t, storm::expressions::Expression>> destinationLocationsAndProbabilities;
            std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>(newGuard);
            TemplateEdgeDestination ted((OrderedAssignments()));
            templateEdge->addDestination(ted);
            destinationLocationsAndProbabilities.emplace_back(sinkIndex, model.getExpressionManager().rational(1.0));

            automaton.addEdge(storm::jani::Edge(i, 0, boost::none, templateEdge, destinationLocationsAndProbabilities));
        } else {
            STORM_LOG_TRACE("\tLocation " + automaton.getLocation(i).getName() + " has no missing guard");
        }
    }
}

void JaniLocalEliminator::Session::buildAutomataInfo() {
    for (auto &aut : model.getAutomata()) {
        automataInfo[aut.getName()] = AutomatonInfo();
        for (auto &loc : aut.getLocations()) {
            bool isPartOfProp = computeIsPartOfProp(aut.getName(), loc.getName());
            setPartOfProp(aut.getName(), loc.getName(), isPartOfProp);
        }
    }
}

JaniLocalEliminator::AutomatonInfo &JaniLocalEliminator::Session::getAutomatonInfo(const std::string &name) {
    return automataInfo[name];
}

JaniLocalEliminator::AutomatonInfo::AutomatonInfo() : hasSink(false), sinkIndex(0) {}
}  // namespace jani
}  // namespace storm
