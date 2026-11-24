#pragma once

#include <memory>
#include <vector>

#include "storm/expressions/Expression.h"
#include "storm/languages/jani/TemplateEdgeDestination.h"
#include "storm/storage/BoostTypes.h"

namespace storm {
namespace jani {
class Model;

class TemplateEdge {
   public:
    TemplateEdge() = default;
    TemplateEdge(TemplateEdge const&) = default;
    TemplateEdge(storm::expressions::Expression const& guard);
    TemplateEdge(storm::expressions::Expression const& guard, OrderedAssignments const& assignments, std::vector<TemplateEdgeDestination> const& destinations);

    storm::expressions::Expression const& getGuard() const;
    void setGuard(storm::expressions::Expression const& newGuard);

    void addDestination(TemplateEdgeDestination const& destination);

    /*!
     * Finalizes the building of this edge. Subsequent changes to the edge require another call to this
     * method. Note that this method is invoked by a call to <code>finalize</code> to the containing model.
     */
    void finalize(Model const& containingModel);

    std::size_t getNumberOfDestinations() const;
    std::vector<TemplateEdgeDestination> const& getDestinations() const;
    std::vector<TemplateEdgeDestination>& getDestinations();
    TemplateEdgeDestination const& getDestination(uint64_t index) const;

    OrderedAssignments const& getAssignments() const;
    OrderedAssignments& getAssignments();

    /*!
     * Adds a transient assignment to this edge.
     *
     * @param assignment The transient assignment to add.
     * @param addToExisting Determines if adding the assigned expression to an already existing assignment is
     * allowed (if the assigned variable is quantitative).
     * @return True if the assignment was added.
     */
    bool addTransientAssignment(Assignment const& assignment, bool addToExisting = false);

    /*!
     * Retrieves a set of (global) variables that are written by at least one of the edge's destinations.
     */
    storm::storage::FlatSet<storm::expressions::Variable> const& getWrittenGlobalVariables() const;

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Changes all variables in assignments based on the given mapping.
     */
    void changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping);

    /*!
     * Finds the transient assignments common to all destinations and lifts them to the edge. Afterwards, these
     * assignments are no longer contained in the destination. Note that this may modify the semantics of the
     * model if assignment levels are being used somewhere in the model.
     */
    void liftTransientDestinationAssignments(int64_t maxLevel = 0);

    /**
     * Shifts the assingments from the edges to the destinations.
     */
    void pushAssignmentsToDestinations();

    /*!
     * Checks whether the provided variables appear on the right-hand side of non-transient assignments.
     */
    bool usesVariablesInNonTransientAssignments(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Retrieves whether there is any transient edge destination assignment in the edge.
     */
    bool hasTransientEdgeDestinationAssignments() const;

    /*!
     * Retrieves whether the edge uses an assignment level other than zero.
     */
    bool usesAssignmentLevels(bool onlyTransient = false) const;

    /*!
     * Retrieves the lowest assignment level occurring in a destination assignment.
     * If no assignment exists, this value is the highest possible integer
     */
    int64_t const& getLowestAssignmentLevel() const;

    /*!
     * Retrieves the highest assignment level occurring in a destination assignment
     * If no assignment exists, this value is always zero
     */
    int64_t const& getHighestAssignmentLevel() const;

    /*!
     * Checks the template edge for linearity.
     */
    bool isLinear() const;

    bool hasEdgeDestinationAssignments() const;

    /*!
     * Simplify Indexed Assignments
     */
    TemplateEdge simplifyIndexedAssignments(bool syncronized, VariableSet const& localVars) const;

   private:
    // The guard of the template edge.
    storm::expressions::Expression guard;

    // The destinations of the template edge.
    std::vector<TemplateEdgeDestination> destinations;

    /// The assignments made when taking this edge.
    OrderedAssignments assignments;

    int64_t lowestAssignmentLevel, highestAssignmentLevel;

    /// A set of global variables that is written by at least one of the edge's destinations. This set is
    /// initialized by the call to <code>finalize</code>.
    storm::storage::FlatSet<storm::expressions::Variable> writtenGlobalVariables;
};

}  // namespace jani
}  // namespace storm
