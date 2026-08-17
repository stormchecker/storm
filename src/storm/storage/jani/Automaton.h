#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/jani/EdgeContainer.h"
#include "storm/storage/jani/FunctionDefinition.h"
#include "storm/storage/jani/TemplateEdgeContainer.h"
#include "storm/storage/jani/VariableSet.h"

namespace storm {
namespace jani {

class Automaton;
class Edge;
class TemplateEdge;
class Location;

class Model;

class Automaton {
   public:
    friend class detail::Edges;
    friend class detail::ConstEdges;

    typedef detail::Edges Edges;
    typedef detail::ConstEdges ConstEdges;

    /*!
     * Creates an empty automaton.
     */
    Automaton(std::string const& name, storm::expressions::Variable const& locationExpressionVariable);

    Automaton(Automaton const& other) = default;
    Automaton& operator=(Automaton const& other) = default;
    Automaton(Automaton&& other) = default;
    Automaton& operator=(Automaton&& other) = default;

    Automaton clone(storm::expressions::ExpressionManager& manager, std::string const& nameOfClone, std::string const& variablePrefix) const;

    /*!
     * Retrieves the name of the automaton.
     */
    std::string const& getName() const;

    /*!
     * Adds the given variable to this automaton
     */
    Variable const& addVariable(Variable const& variable);

    /*!
     * Retrieves the variables of this automaton.
     */
    VariableSet& getVariables();

    /*!
     * Retrieves the variables of this automaton.
     */
    VariableSet const& getVariables() const;

    bool hasVariable(std::string const& name) const;

    /*!
     * Retrieves all expression variables used by this automaton.
     *
     * @return The set of expression variables used by this automaton.
     */
    std::set<storm::expressions::Variable> getAllExpressionVariables() const;

    /*!
     * Retrieves whether this automaton has a transient variable.
     */
    bool hasTransientVariable() const;

    /*!
     * Adds the given function definition
     */
    FunctionDefinition const& addFunctionDefinition(FunctionDefinition const& functionDefinition);

    /*!
     * Retrieves all function definitions of this automaton
     */
    std::unordered_map<std::string, FunctionDefinition> const& getFunctionDefinitions() const;

    /*!
     * Retrieves all function definitions of this automaton
     */
    std::unordered_map<std::string, FunctionDefinition>& getFunctionDefinitions();

    /*!
     * Retrieves whether the automaton has a location with the given name.
     */
    bool hasLocation(std::string const& name) const;

    /*!
     * Get location id for a location with a given name.
     * Yields undefined behaviour if no such location exists;
     *
     * @name the name of the location
     */
    uint64_t getLocationIndex(std::string const& name) const;

    /*!
     * Retrieves the locations of the automaton.
     */
    std::vector<Location> const& getLocations() const;

    /*!
     * Retrieves the locations of the automaton.
     */
    std::vector<Location>& getLocations();

    /*!
     * Retrieves the location with the given index.
     */
    Location const& getLocation(uint64_t index) const;

    /*!
     * Retrieves the location with the given index.
     */
    Location& getLocation(uint64_t index);

    /*!
     * Adds the given location to the automaton.
     */
    uint64_t addLocation(Location const& location);

    /*!
     * Adds the location with the given name to the initial locations.
     */
    void addInitialLocation(std::string const& name);

    /*!
     * Adds the location with the given index to the initial locations.
     */
    void addInitialLocation(uint64_t index);

    /*!
     * Retrieves the indices of the initial locations.
     */
    std::set<uint64_t> const& getInitialLocationIndices() const;

    /*!
     * Builds a map from ID to Location Name.
     */
    std::map<uint64_t, std::string> buildIdToLocationNameMap() const;

    /*!
     * Retrieves the expression variable that represents the location of this automaton.
     */
    storm::expressions::Variable const& getLocationExpressionVariable() const;

    /*!
     * Retrieves the edge with the given index in this automaton.
     */
    Edge const& getEdge(uint64_t index) const;

    /*!
     * Retrieves the edges of the location with the given name.
     */
    Edges getEdgesFromLocation(std::string const& name);

    /*!
     * Retrieves the edges of the location with the given index.
     */
    Edges getEdgesFromLocation(uint64_t index);

    /*!
     * Retrieves the edges of the location with the given name.
     */
    ConstEdges getEdgesFromLocation(std::string const& name) const;

    /*!
     * Retrieves the edges of the location with the given index.
     */
    ConstEdges getEdgesFromLocation(uint64_t index) const;

    /*!
     * Retrieves the edges of the location with the given index labeled with the given action index.
     */
    Edges getEdgesFromLocation(uint64_t locationIndex, uint64_t actionIndex);

    /*!
     * Retrieves the edges of the location with the given index.
     */
    ConstEdges getEdgesFromLocation(uint64_t locationIndex, uint64_t actionIndex) const;

    /*!
     * Retrieves the container of all edges of this automaton.
     */
    EdgeContainer const& getEdgeContainer() const;

    /*!
     * Retrieves the container of all edges of this automaton.
     */
    EdgeContainer& getEdgeContainer();

    /*!
     * Adds the template edge to the list of edges
     */
    void registerTemplateEdge(std::shared_ptr<TemplateEdge> const&);

    /*!
     * Adds an edge to the automaton.
     */
    void addEdge(Edge const& edge);

    bool validate() const;

    /*!
     * Retrieves the edges of the automaton.
     */
    std::vector<Edge>& getEdges();

    /*!
     * Retrieves the edges of the automaton.
     */
    std::vector<Edge> const& getEdges() const;

    /*!
     * Retrieves the set of action indices that are labels of edges of this automaton.
     */
    std::set<uint64_t> getActionIndices() const;

    /*!
     * Retrieves the number of locations.
     */
    uint64_t getNumberOfLocations() const;

    /*!
     * Retrieves the number of edges.
     */
    uint64_t getNumberOfEdges() const;

    /*!
     * Retrieves whether the initial restriction is set and unequal to true
     */
    bool hasRestrictedInitialStates() const;

    /*!
     * Retrieves whether this automaton has an initial states restriction.
     */
    bool hasInitialStatesRestriction() const;

    /*!
     * Retrieves whether this automaton has non-trivial initial states.
     */
    bool hasNonTrivialInitialStates() const;

    /*!
     * Gets the expression restricting the legal initial values of the automaton's variables.
     */
    storm::expressions::Expression const& getInitialStatesRestriction() const;

    /*!
     * Sets the expression restricting the legal initial values of the automaton's variables.
     */
    void setInitialStatesRestriction(storm::expressions::Expression const& initialStatesRestriction);

    /*!
     * Retrieves the expression defining the legal initial values of the automaton's variables.
     */
    storm::expressions::Expression getInitialStatesExpression() const;

    /*!
     * Retrieves whether the initial states expression is trivial in the sense that the automaton has no initial
     * states restriction and all non-transient variables have initial values.
     */
    bool hasTrivialInitialStatesExpression() const;

    /*!
     * Retrieves whether there is an edge labeled with the action with the given index in this automaton.
     */
    bool hasEdgeLabeledWithActionIndex(uint64_t actionIndex) const;

    /*!
     * Retrieves a list of expressions that characterize the legal values of the variables in this automaton.
     */
    std::vector<storm::expressions::Expression> getAllRangeExpressions() const;

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Changes all variables in assignments based on the given mapping.
     */
    void changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping);

    /*!
     * Finalizes the building of this automaton. Subsequent changes to the automaton require another call to this
     * method. Note that this method is invoked by a call to <code>finalize</code> to the containing model.
     */
    void finalize(Model const& containingModel);

    /*!
     * Checks whether the provided variables only appear in the probability expressions or the expressions being
     * assigned in transient assignments.
     */
    bool containsVariablesOnlyInProbabilitiesOrTransientAssignments(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Pushes the edge assignments to the corresponding destinations.
     */
    void pushEdgeAssignmentsToDestinations();

    /*!
     * Pushes the assignments to real-valued transient variables to the edges.
     * Note: This is currently only supported if the template edges are uniquely coupled with one source location.
     */
    void pushTransientRealLocationAssignmentsToEdges();

    /*!
     * Retrieves whether there is any transient edge destination assignment in the automaton.
     */
    bool hasTransientEdgeDestinationAssignments() const;

    /*!
     * Lifts the common edge destination assignments to edge assignments.
     */
    void liftTransientEdgeDestinationAssignments(int64_t maxLevel = 0);

    /*!
     * Retrieves whether the automaton uses an assignment level other than zero.
     */
    bool usesAssignmentLevels(bool onlyTransient = false) const;

    /*!
     * Checks the automaton for linearity.
     */
    bool isLinear() const;

    void writeDotToStream(std::ostream& outStream, std::vector<std::string> const& actionNames) const;

    /*!
     * Restricts the automaton to the edges given by the indices. All other edges are deleted.
     */
    void restrictToEdges(storm::storage::FlatSet<uint_fast64_t> const& edgeIndices);

   private:
    /// The name of the automaton.
    std::string name;

    /// The expression variable representing the location of this automaton.
    storm::expressions::Variable locationExpressionVariable;

    /// The set of variables of this automaton.
    VariableSet variables;

    /// A mapping from names to function definitions
    /// Since we use an unordered_map, references to function definitions will not get invalidated when more function definitions are added
    std::unordered_map<std::string, FunctionDefinition> functionDefinitions;

    /// The locations of the automaton.
    std::vector<Location> locations;

    /// A mapping of location names to their indices.
    std::unordered_map<std::string, uint64_t> locationToIndex;

    /// All edges of the automaton
    EdgeContainer edges;

    /// A mapping from location indices to the starting indices. If l is mapped to i, it means that the edges
    /// leaving location l start at index i of the edges vector.
    std::vector<uint64_t> locationToStartingIndex;

    /// The indices of the initial locations.
    std::set<uint64_t> initialLocationIndices;

    /// The expression restricting the legal initial values of the variables of the automaton.
    storm::expressions::Expression initialStatesRestriction;

    /// The set of action indices that label some action in this automaton.
    std::set<uint64_t> actionIndices;
};

}  // namespace jani
}  // namespace storm
