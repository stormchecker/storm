#include "storm/storage/jani/Automaton.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/TemplateEdge.h"
#include "storm/storage/jani/types/AllJaniTypes.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

Automaton::Automaton(std::string const& name, storm::expressions::Variable const& locationExpressionVariable)
    : name(name), locationExpressionVariable(locationExpressionVariable) {
    // Add a sentinel element to the mapping from locations to starting indices.
    locationToStartingIndex.push_back(0);
}

storm::expressions::Variable cloneVariable(storm::expressions::ExpressionManager& manager, storm::expressions::Variable const& var,
                                           std::string const& variablePrefix) {
    STORM_LOG_ASSERT(var.getManager() == manager, "Expected same manager.");
    return manager.declareVariable(variablePrefix + var.getName(), var.getType());
}

Automaton Automaton::clone(storm::expressions::ExpressionManager& manager, std::string const& nameOfClone, std::string const& variablePrefix) const {
    Automaton result(*this);
    result.name = nameOfClone;
    result.locationExpressionVariable = cloneVariable(manager, result.locationExpressionVariable, variablePrefix);
    auto allVars = result.getAllExpressionVariables();
    std::map<storm::expressions::Variable, storm::expressions::Expression> oldToNewVarMap;
    for (auto const& v : allVars) {
        oldToNewVarMap[v] = cloneVariable(manager, v, variablePrefix).getExpression();
    }
    result.variables.substituteExpressionVariables(oldToNewVarMap);
    // When cloning an automaton, keep the transcendental numbers as they are.
    const bool substituteTranscendentalNumbers = false;
    result.substitute(oldToNewVarMap, substituteTranscendentalNumbers);
    return result;
}

std::string const& Automaton::getName() const {
    return name;
}

Variable const& Automaton::addVariable(Variable const& variable) {
    return variables.addVariable(variable);
}

bool Automaton::hasVariable(std::string const& name) const {
    return variables.hasVariable(name);
}

VariableSet& Automaton::getVariables() {
    return variables;
}

VariableSet const& Automaton::getVariables() const {
    return variables;
}

std::set<storm::expressions::Variable> Automaton::getAllExpressionVariables() const {
    std::set<storm::expressions::Variable> result;
    for (auto const& variable : this->getVariables()) {
        result.insert(variable.getExpressionVariable());
    }
    return result;
}

bool Automaton::hasTransientVariable() const {
    return variables.hasTransientVariable();
}

FunctionDefinition const& Automaton::addFunctionDefinition(FunctionDefinition const& functionDefinition) {
    auto insertionRes = functionDefinitions.emplace(functionDefinition.getName(), functionDefinition);
    STORM_LOG_THROW(insertionRes.second, storm::exceptions::InvalidArgumentException,
                    "A function with the name " << functionDefinition.getName() << " already exists in this automaton (" << this->getName() << ").");
    return insertionRes.first->second;
}

std::unordered_map<std::string, FunctionDefinition> const& Automaton::getFunctionDefinitions() const {
    return functionDefinitions;
}

std::unordered_map<std::string, FunctionDefinition>& Automaton::getFunctionDefinitions() {
    return functionDefinitions;
}

bool Automaton::hasLocation(std::string const& name) const {
    return locationToIndex.find(name) != locationToIndex.end();
}

std::vector<Location> const& Automaton::getLocations() const {
    return locations;
}

std::vector<Location>& Automaton::getLocations() {
    return locations;
}

Location const& Automaton::getLocation(uint64_t index) const {
    return locations[index];
}

Location& Automaton::getLocation(uint64_t index) {
    return locations[index];
}

uint64_t Automaton::addLocation(Location const& location) {
    STORM_LOG_THROW(!this->hasLocation(location.getName()), storm::exceptions::WrongFormatException,
                    "Cannot add location with name '" << location.getName() << "', because a location with this name already exists.");
    locationToIndex.emplace(location.getName(), locations.size());
    locations.push_back(location);
    locationToStartingIndex.push_back(edges.size());
    return locations.size() - 1;
}

uint64_t Automaton::getLocationIndex(std::string const& name) const {
    STORM_LOG_ASSERT(hasLocation(name), "Location not found.");
    return locationToIndex.at(name);
}

void Automaton::addInitialLocation(std::string const& name) {
    auto it = locationToIndex.find(name);
    STORM_LOG_THROW(it != locationToIndex.end(), storm::exceptions::InvalidArgumentException,
                    "Cannot make unknown location '" << name << "' the initial location.");
    return addInitialLocation(it->second);
}

void Automaton::addInitialLocation(uint64_t index) {
    STORM_LOG_THROW(index < locations.size(), storm::exceptions::InvalidArgumentException,
                    "Cannot make location with index " << index << " initial: out of bounds.");
    initialLocationIndices.insert(index);
}

std::set<uint64_t> const& Automaton::getInitialLocationIndices() const {
    return initialLocationIndices;
}

std::map<uint64_t, std::string> Automaton::buildIdToLocationNameMap() const {
    std::map<uint64_t, std::string> mapping;
    uint64_t i = 0;
    for (auto const& loc : locations) {
        mapping[i] = loc.getName();
        ++i;
    }
    return mapping;
}

storm::expressions::Variable const& Automaton::getLocationExpressionVariable() const {
    return locationExpressionVariable;
}

Edge const& Automaton::getEdge(uint64_t index) const {
    return edges.getConcreteEdges()[index];
}

Automaton::Edges Automaton::getEdgesFromLocation(std::string const& name) {
    auto it = locationToIndex.find(name);
    STORM_LOG_THROW(it != locationToIndex.end(), storm::exceptions::InvalidArgumentException, "Cannot retrieve edges from unknown location '" << name << ".");
    return getEdgesFromLocation(it->second);
}

Automaton::Edges Automaton::getEdgesFromLocation(uint64_t index) {
    auto it = edges.begin();
    std::advance(it, locationToStartingIndex[index]);
    auto ite = edges.begin();
    std::advance(ite, locationToStartingIndex[index + 1]);
    return Edges(it, ite);
}

Automaton::ConstEdges Automaton::getEdgesFromLocation(std::string const& name) const {
    auto it = locationToIndex.find(name);
    STORM_LOG_THROW(it != locationToIndex.end(), storm::exceptions::InvalidArgumentException, "Cannot retrieve edges from unknown location '" << name << ".");
    return getEdgesFromLocation(it->second);
}

Automaton::ConstEdges Automaton::getEdgesFromLocation(uint64_t index) const {
    auto it = edges.begin();
    std::advance(it, locationToStartingIndex[index]);
    auto ite = edges.begin();
    std::advance(ite, locationToStartingIndex[index + 1]);
    return ConstEdges(it, ite);
}

Automaton::Edges Automaton::getEdgesFromLocation(uint64_t locationIndex, uint64_t actionIndex) {
    typedef std::vector<Edge>::iterator ForwardIt;

    // Perform binary search for start of edges with the given action index.
    auto first = edges.begin();
    std::advance(first, locationToStartingIndex[locationIndex]);
    auto last = edges.begin();
    std::advance(last, locationToStartingIndex[locationIndex + 1]);
    typename std::iterator_traits<ForwardIt>::difference_type count, step;
    count = std::distance(first, last);

    ForwardIt it1;
    while (count > 0) {
        it1 = first;
        step = count / 2;
        std::advance(it1, step);
        if (it1->getActionIndex() < actionIndex) {
            first = ++it1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    it1 = first;

    // If there is no such edge, we can return now.
    if (it1 != last && it1->getActionIndex() > actionIndex) {
        return Edges(last, last);
    }

    // Otherwise, perform a binary search for the end of the edges with the given action index.
    count = std::distance(it1, last);

    ForwardIt it2;
    while (count > 0) {
        it2 = it1;
        step = count / 2;
        std::advance(it2, step);
        if (actionIndex >= it2->getActionIndex()) {
            first = ++it2;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    it2 = first;

    return Edges(it1, it2);
}

Automaton::ConstEdges Automaton::getEdgesFromLocation(uint64_t locationIndex, uint64_t actionIndex) const {
    typedef std::vector<Edge>::const_iterator ForwardIt;

    // Perform binary search for start of edges with the given action index.
    auto first = edges.begin();
    std::advance(first, locationToStartingIndex[locationIndex]);
    auto last = edges.begin();
    std::advance(last, locationToStartingIndex[locationIndex + 1]);
    typename std::iterator_traits<ForwardIt>::difference_type count, step;
    count = std::distance(first, last);

    ForwardIt it1;
    while (count > 0) {
        it1 = first;
        step = count / 2;
        std::advance(it1, step);
        if (it1->getActionIndex() < actionIndex) {
            first = ++it1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    it1 = first;

    // If there is no such edge, we can return now.
    if (it1 != last && it1->getActionIndex() > actionIndex) {
        return ConstEdges(last, last);
    }

    // Otherwise, perform a binary search for the end of the edges with the given action index.
    count = std::distance(it1, last);

    ForwardIt it2;
    while (count > 0) {
        it2 = first;
        step = count / 2;
        std::advance(it2, step);
        if (actionIndex >= it2->getActionIndex()) {
            first = ++it2;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    it2 = first;

    return ConstEdges(it1, it2);
}

EdgeContainer const& Automaton::getEdgeContainer() const {
    return edges;
}

EdgeContainer& Automaton::getEdgeContainer() {
    return edges;
}

void Automaton::addEdge(Edge const& edge) {
    STORM_LOG_THROW(edge.getSourceLocationIndex() < locations.size(), storm::exceptions::InvalidArgumentException,
                    "Cannot add edge with unknown source location index '" << edge.getSourceLocationIndex() << "'.");
    STORM_LOG_ASSERT(validate(), "Automaton validation failed.");

    edges.insertEdge(edge, locationToStartingIndex[edge.getSourceLocationIndex()], locationToStartingIndex[edge.getSourceLocationIndex() + 1]);
    // Update the set of action indices of this automaton.
    actionIndices.insert(edge.getActionIndex());

    // Now update the starting indices of all subsequent locations.
    for (uint64_t locationIndex = edge.getSourceLocationIndex() + 1; locationIndex < locationToStartingIndex.size(); ++locationIndex) {
        ++locationToStartingIndex[locationIndex];
    }
}

std::vector<Edge>& Automaton::getEdges() {
    return edges.getConcreteEdges();
}

std::vector<Edge> const& Automaton::getEdges() const {
    return edges.getConcreteEdges();
}

std::set<uint64_t> Automaton::getActionIndices() const {
    return edges.getActionIndices();
}

uint64_t Automaton::getNumberOfLocations() const {
    return locations.size();
}

uint64_t Automaton::getNumberOfEdges() const {
    return edges.size();
}

bool Automaton::hasRestrictedInitialStates() const {
    if (!hasInitialStatesRestriction()) {
        return false;
    }
    if (getInitialStatesRestriction().containsVariables()) {
        return true;
    } else {
        return !getInitialStatesRestriction().evaluateAsBool();
    }
}

bool Automaton::hasInitialStatesRestriction() const {
    return initialStatesRestriction.isInitialized();
}

bool Automaton::hasNonTrivialInitialStates() const {
    if (this->hasInitialStatesRestriction() && !this->getInitialStatesRestriction().isTrue()) {
        return true;
    }

    for (auto const& variable : this->getVariables()) {
        if (variable.hasInitExpression() && !variable.isTransient()) {
            return true;
        }
    }

    return false;
}

storm::expressions::Expression const& Automaton::getInitialStatesRestriction() const {
    return initialStatesRestriction;
}

void Automaton::setInitialStatesRestriction(storm::expressions::Expression const& initialStatesRestriction) {
    this->initialStatesRestriction = initialStatesRestriction;
}

storm::expressions::Expression Automaton::getInitialStatesExpression() const {
    storm::expressions::Expression result;

    // Add initial state restriction if there is one.
    if (this->hasInitialStatesRestriction() && !this->getInitialStatesRestriction().isTrue()) {
        result = this->getInitialStatesRestriction();
    }

    // Add the expressions for all non-transient variables that have initial expressions.
    for (auto const& variable : this->getVariables()) {
        if (variable.isTransient()) {
            continue;
        }

        if (variable.hasInitExpression()) {
            storm::expressions::Expression newInitExpression;
            if (variable.getType().isBasicType() && variable.getType().asBasicType().isBooleanType()) {
                newInitExpression = storm::expressions::iff(variable.getExpressionVariable(), variable.getInitExpression());
            } else {
                newInitExpression = variable.getExpressionVariable() == variable.getInitExpression();
            }
            if (result.isInitialized()) {
                result = result && newInitExpression;
            } else {
                result = newInitExpression;
            }
        }
    }

    return result;
}

bool Automaton::hasTrivialInitialStatesExpression() const {
    if (this->hasInitialStatesRestriction()) {
        return false;
    }

    bool result = true;
    for (auto const& variable : this->getVariables()) {
        if (variable.isTransient()) {
            continue;
        }

        result &= variable.hasInitExpression();

        if (!result) {
            break;
        }
    }

    return result;
}

bool Automaton::hasEdgeLabeledWithActionIndex(uint64_t actionIndex) const {
    return actionIndices.find(actionIndex) != actionIndices.end();
}

std::vector<storm::expressions::Expression> Automaton::getAllRangeExpressions() const {
    std::vector<storm::expressions::Expression> result;
    for (auto const& variable : this->getVariables().getBoundedIntegerVariables()) {
        result.push_back(variable.getRangeExpression());
    }
    return result;
}

void Automaton::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                           bool const substituteTranscendentalNumbers) {
    for (auto& functionDefinition : this->getFunctionDefinitions()) {
        functionDefinition.second.substitute(substitution, substituteTranscendentalNumbers);
    }

    this->getVariables().substitute(substitution, substituteTranscendentalNumbers);

    for (auto& location : this->getLocations()) {
        location.substitute(substitution, substituteTranscendentalNumbers);
    }

    if (hasInitialStatesRestriction()) {
        this->setInitialStatesRestriction(substituteJaniExpression(this->getInitialStatesRestriction(), substitution, substituteTranscendentalNumbers));
    }

    edges.substitute(substitution, substituteTranscendentalNumbers);
}
void Automaton::registerTemplateEdge(std::shared_ptr<TemplateEdge> const& te) {
    edges.insertTemplateEdge(te);
}

void Automaton::changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) {
    for (auto& location : locations) {
        location.changeAssignmentVariables(remapping);
    }
    edges.changeAssignmentVariables(remapping);
}

void Automaton::finalize(Model const& containingModel) {
    // simplifyIndexedAssignments();
    edges.finalize(containingModel);
}

bool Automaton::containsVariablesOnlyInProbabilitiesOrTransientAssignments(std::set<storm::expressions::Variable> const& variables) const {
    // Check initial states restriction expression.
    if (this->hasInitialStatesRestriction()) {
        if (this->getInitialStatesRestriction().containsVariable(variables)) {
            return false;
        }
    }

    // Check global variable definitions.
    if (this->getVariables().containsVariablesInBoundExpressionsOrInitialValues(variables)) {
        return false;
    }

    // Check edges.
    for (auto const& edge : edges) {
        if (edge.usesVariablesInNonTransientAssignments(variables)) {
            return false;
        }
    }

    return true;
}

void Automaton::pushEdgeAssignmentsToDestinations() {
    edges.pushAssignmentsToDestinations();
}

void Automaton::pushTransientRealLocationAssignmentsToEdges() {
    std::set<std::shared_ptr<storm::jani::TemplateEdge>> encounteredTemplateEdges;

    for (uint64_t locationIndex = 0; locationIndex < locations.size(); ++locationIndex) {
        auto& location = locations[locationIndex];
        auto edges = this->getEdgesFromLocation(locationIndex);

        storm::jani::Location newLocation(location.getName());
        bool createNewLocation = true;
        for (auto& edge : edges) {
            STORM_LOG_THROW(encounteredTemplateEdges.find(edge.getTemplateEdge()) == encounteredTemplateEdges.end(), storm::exceptions::NotSupportedException,
                            "Pushing location assignments to edges is only supported for automata with unique template edges.");

            auto& templateEdge = edge.getTemplateEdge();
            encounteredTemplateEdges.insert(templateEdge);

            for (auto const& assignment : location.getAssignments().getTransientAssignments()) {
                auto const& var = assignment.getVariable();
                if (var.isTransient() && var.getType().isBasicType() && var.getType().asBasicType().isRealType()) {
                    templateEdge->addTransientAssignment(assignment, true);
                } else if (createNewLocation) {
                    newLocation.addTransientAssignment(assignment);
                }
            }

            if (createNewLocation) {
                createNewLocation = false;
            }
        }

        location = std::move(newLocation);
    }
}

bool Automaton::hasTransientEdgeDestinationAssignments() const {
    for (auto const& edge : this->getEdges()) {
        if (edge.hasTransientEdgeDestinationAssignments()) {
            return true;
        }
    }
    return false;
}

void Automaton::liftTransientEdgeDestinationAssignments(int64_t maxLevel) {
    edges.liftTransientDestinationAssignments(maxLevel);
}

bool Automaton::validate() const {
    STORM_LOG_ASSERT(locationToStartingIndex.size() == locations.size() + 1, "Location index size mismatch.");
    for (uint64_t i = 0; i < locations.size(); i++) {
        STORM_LOG_ASSERT(locationToStartingIndex[i] <= locationToStartingIndex[i + 1], "Location index not monotonically increasing.");
    }
    return true;
}

bool Automaton::usesAssignmentLevels(bool onlyTransient) const {
    return edges.usesAssignmentLevels(onlyTransient);
}

bool Automaton::isLinear() const {
    bool result = true;

    for (auto const& location : this->getLocations()) {
        result &= location.isLinear();
    }
    if (result) {
        result &= edges.isLinear();
    }
    return result;
}

void Automaton::restrictToEdges(storm::storage::FlatSet<uint_fast64_t> const& edgeIndices) {
    std::vector<Edge> oldEdges = this->edges.getConcreteEdges();

    this->edges.clearConcreteEdges();
    actionIndices.clear();
    for (auto& e : locationToStartingIndex) {
        e = 0;
    }

    for (auto const& index : edgeIndices) {
        this->addEdge(oldEdges[index]);
    }
}

void Automaton::writeDotToStream(std::ostream& outStream, std::vector<std::string> const& actionNames) const {
    outStream << "\tsubgraph " << name << " {\n";

    // Write all locations to the stream.
    uint64_t locIndex = 0;
    for (auto const& loc : locations) {
        outStream << "\t" << name << "_s" << locIndex << "[ label=\"" << loc.getName() << "\"];\n";
        ++locIndex;
    }
    // Write for each edge an node to the stream;
    uint64_t edgeIndex = 0;
    for (auto const& edge : edges) {
        outStream << "\t" << name << "_e" << edgeIndex << "[ label=\"\" , shape=circle, width=.2, style=filled, fillcolor=\"black\"];\n";
        ++edgeIndex;

        // Silencing unused variable warning.
        (void)edge;
    }

    // Connect edges
    edgeIndex = 0;
    for (auto const& edge : edges) {
        outStream << "\t" << name << "_s" << edge.getSourceLocationIndex() << " -> " << name << "_e" << edgeIndex << " [label=\""
                  << actionNames.at(edge.getActionIndex()) << "\"];\n";
        for (auto const& edgeDest : edge.getDestinations()) {
            outStream << "\t" << name << "_e" << edgeIndex << " -> " << name << "_s" << edgeDest.getLocationIndex() << ";\n";
        }
        ++edgeIndex;
    }

    outStream << "\t}\n";
}
}  // namespace jani
}  // namespace storm
