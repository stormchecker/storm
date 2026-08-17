#pragma once

#include <memory>

#include <boost/optional.hpp>
#include <iostream>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/OrderedAssignments.h"

namespace storm {
namespace jani {

class TemplateEdge;

class Edge {
   public:
    Edge() = default;

    Edge(uint64_t sourceLocationIndex, uint64_t actionIndex, boost::optional<storm::expressions::Expression> const& rate,
         std::shared_ptr<TemplateEdge> const& templateEdge,
         std::vector<std::pair<uint64_t, storm::expressions::Expression>> const& destinationTargetLocationsAndProbabilities);
    Edge(uint64_t sourceLocationIndex, uint64_t actionIndex, boost::optional<storm::expressions::Expression> const& rate,
         std::shared_ptr<TemplateEdge> const& templateEdge, std::vector<uint64_t> const& destinationLocations,
         std::vector<storm::expressions::Expression> const& destinationProbabilities);

    /*!
     * Retrieves the index of the source location.
     */
    uint64_t getSourceLocationIndex() const;

    /*!
     * Retrieves the id of the action with which this edge is labeled.
     */
    uint64_t getActionIndex() const;

    /*!
     * Returns whether it contains the silent action.
     */
    bool hasSilentAction() const;

    /*!
     * Retrieves whether this edge has an associated rate.
     */
    bool hasRate() const;

    /*!
     * Retrieves the rate of this edge. Note that calling this is only valid if the edge has an associated rate.
     */
    storm::expressions::Expression const& getRate() const;

    /*!
     * Retrieves an optional that stores the rate if there is any and none otherwise.
     */
    boost::optional<storm::expressions::Expression> const& getOptionalRate() const;

    /*!
     * Sets a new rate for this edge.
     */
    void setRate(storm::expressions::Expression const& rate);

    /*!
     * Retrieves the guard of this edge.
     */
    storm::expressions::Expression const& getGuard() const;

    /*!
     * Sets a new guard for this edge.
     */
    void setGuard(storm::expressions::Expression const& guard);

    /*!
     * Retrieves the destination with the given index.
     */
    EdgeDestination const& getDestination(uint64_t index) const;

    /*!
     * Retrieves the destinations of this edge.
     */
    std::vector<EdgeDestination> const& getDestinations() const;

    /*!
     * Retrieves the destinations of this edge.
     */
    std::vector<EdgeDestination>& getDestinations();

    /*!
     * Retrieves the number of destinations of this edge.
     */
    std::size_t getNumberOfDestinations() const;

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Retrieves a set of (global) variables that are written by at least one of the edge's destinations.
     */
    storm::storage::FlatSet<storm::expressions::Variable> const& getWrittenGlobalVariables() const;

    /*!
     * Retrieves the assignments of this edge.
     */
    OrderedAssignments const& getAssignments() const;

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
     * Retrieves the color of the edge
     */
    uint64_t getColor() const;

    /*!
     * Sets the color of the edge
     */
    void setColor(uint64_t newColor);

    /*!
     *
     * @param localVars
     */
    void simplifyIndexedAssignments(VariableSet const& localVars);

    std::shared_ptr<TemplateEdge> const& getTemplateEdge();
    void setTemplateEdge(std::shared_ptr<TemplateEdge> const& newTe);

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

    std::string toString() const;

   private:
    /// The index of the source location.
    uint64_t sourceLocationIndex;

    /// The index of the action with which this edge is labeled.
    uint64_t actionIndex;

    /// The rate with which this edge is taken. This only applies to continuous-time models. For discrete-time
    /// models, this must be set to none.
    boost::optional<storm::expressions::Expression> rate;

    /// The template of this edge: guards and destinations. Notice that after finalizing, the template edge might be reused; changing it is not permitted.
    std::shared_ptr<TemplateEdge> templateEdge;

    /// The concrete destination objects.
    std::vector<EdgeDestination> destinations;

    /// The color of the edge, used to persistently mark and identify specific edges (by the user)
    uint64_t color = 0;
};

std::ostream& operator<<(std::ostream& stream, Edge const& edge);

}  // namespace jani
}  // namespace storm
