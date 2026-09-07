#pragma once

#include <cstdint>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/jani/TemplateEdgeDestination.h"

namespace storm {
namespace jani {

class EdgeDestination {
   public:
    /*!
     * Creates a new edge destination.
     */
    EdgeDestination(uint64_t locationIndex, storm::expressions::Expression const& probability, TemplateEdgeDestination const& templateEdgeDestination);

    /*!
     * Retrieves the id of the destination location.
     */
    uint64_t getLocationIndex() const;

    /*!
     * Retrieves the probability of choosing this destination.
     */
    storm::expressions::Expression const& getProbability() const;

    /*!
     * Sets a new probability for this edge destination.
     */
    void setProbability(storm::expressions::Expression const& probability);

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Retrieves the mapping from variables to their assigned expressions that corresponds to the assignments
     * of this destination.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getAsVariableToExpressionMap() const;

    /*!
     * Retrieves the assignments to make when choosing this destination.
     */
    OrderedAssignments const& getOrderedAssignments() const;

    /*!
     * Checks whether this destination has the given assignment.
     */
    bool hasAssignment(Assignment const& assignment) const;

    /*!
     * Retrieves whether this destination has transient assignments.
     */
    bool hasTransientAssignment() const;

    /*!
     * Retrieves whether the edge uses an assignment level other than zero.
     */
    bool usesAssignmentLevels() const;

    /*!
     * Retrieves the template destination for this destination.
     */
    TemplateEdgeDestination const& getTemplateEdgeDestination() const;

    void updateTemplateEdgeDestination(TemplateEdgeDestination const& newTed);

   private:
    // The index of the destination location.
    uint64_t locationIndex;

    // The probability to go to the destination.
    storm::expressions::Expression probability;

    // The template edge destination
    std::reference_wrapper<TemplateEdgeDestination const> templateEdgeDestination;
};

}  // namespace jani
}  // namespace storm
