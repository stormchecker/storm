#pragma once

#include <string>

#include "storm/languages/jani/OrderedAssignments.h"

namespace storm {
namespace jani {

/**
 * Jani Location:
 *
 * Whereas Jani Locations also support invariants, we do not have support for them (as we do not support any of the allowed model types).
 */
class Location {
   public:
    /*!
     * Creates a new location.
     */
    Location(std::string const& name, std::vector<Assignment> const& transientAssignments = {});

    Location(std::string const& name, OrderedAssignments const& assignments);
    /*!
     * Retrieves the name of the location.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the assignments of this location.
     */
    OrderedAssignments const& getAssignments() const;

    /*!
     * Retrieves the assignments of this location.
     */
    OrderedAssignments& getAssignments();

    /*!
     * Adds the given transient assignment to this location.
     */
    void addTransientAssignment(storm::jani::Assignment const& assignment);

    /*!
     * Retrieves whether a time progress invariant is attached to this location
     */
    bool hasTimeProgressInvariant() const;

    /*!
     * Retrieves the time progress invariant
     */
    storm::expressions::Expression const& getTimeProgressInvariant() const;

    /*!
     * Sets the time progress invariant of this location
     * @param expression the location invariant (type bool)
     */
    void setTimeProgressInvariant(storm::expressions::Expression const& expression);

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Changes all variables in assignments based on the given mapping.
     */
    void changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping);

    /*!
     * Checks whether the location is valid, that is, whether the assignments are indeed all transient assignments.
     */
    void checkValid() const;

    /*!
     * Checks the automaton for linearity.
     */
    bool isLinear() const;

   private:
    /// The name of the location.
    std::string name;

    /// The transient assignments made in this location.
    OrderedAssignments assignments;

    // The location's time progress condition
    storm::expressions::Expression timeProgressInvariant;
};

}  // namespace jani
}  // namespace storm
