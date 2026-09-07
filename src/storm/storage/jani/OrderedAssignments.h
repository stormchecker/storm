#pragma once

#include "storm/adapters/DereferenceIteratorAdapter.h"
#include "storm/storage/jani/Assignment.h"

namespace storm {
namespace jani {

class VariableSet;

namespace detail {
using Assignments = storm::adapters::DereferenceIteratorAdapter<std::vector<std::shared_ptr<Assignment>>>;
using ConstAssignments = storm::adapters::DereferenceIteratorAdapter<std::vector<std::shared_ptr<Assignment>> const>;
}  // namespace detail

class OrderedAssignments {
   public:
    /*!
     * Creates an ordered set of assignments.
     */
    OrderedAssignments(std::vector<Assignment> const& assignments = std::vector<Assignment>());

    explicit OrderedAssignments(Assignment const& assignment);

    /*!
     * Adds the given assignment to the set of assignments.
     *
     * @addToExisting If true the value of the assigned expression is added to a (potentially) previous assignment
     * to the variable. If false and there is already an assignment, an exception is thrown.
     * @return True iff the assignment was added.
     */
    bool add(Assignment const& assignment, bool addToExisting = false);

    /*!
     * Removes the given assignment from this set of assignments.
     *
     * @return True if the assignment was found and removed.
     */
    bool remove(Assignment const& assignment);

    /*!
     * Checks whether the assignments have several levels.
     *
     * @return True if more than one level occurs in the assignment set.
     */
    bool hasMultipleLevels(bool onlyTransient = false) const;

    /**
     * Produces a new OrderedAssignments object with simplified leveling
     * @param synchronous
     * @param localVars
     * @return
     */
    OrderedAssignments simplifyLevels(bool synchronous, VariableSet const& localVars, bool first = true) const;

    /*!
     * Retrieves whether this set of assignments is empty.
     */
    bool empty(bool onlyTransient = false) const;

    /*!
     * Removes all assignments from this set.
     */
    void clear();

    /*!
     * Retrieves the total number of assignments.
     */
    std::size_t getNumberOfAssignments() const;

    /*!
     * Retrieves the lowest level among all assignments. Note that this may only be called if there is at least
     * one assignment.
     */
    int64_t getLowestLevel(bool onlyTransient = false) const;

    /*!
     * Retrieves the highest level among all assignments. Note that this may only be called if there is at least
     * one assignment.
     */
    int64_t getHighestLevel(bool onlyTransient = false) const;

    /*!
     * Retrieves whether the given assignment is contained in this set of assignments.
     */
    bool contains(Assignment const& assignment) const;

    /*!
     * Returns all assignments in this set of assignments.
     */
    detail::ConstAssignments getAllAssignments() const;

    /*!
     * Returns all transient assignments in this set of assignments.
     */
    detail::ConstAssignments getTransientAssignments() const;

    /*!
     * Returns all transient assignments in this set of assignments.
     */
    detail::ConstAssignments getTransientAssignments(int64_t assignmentLevel) const;

    /*!
     * Returns all non-transient assignments in this set of assignments.
     */
    detail::ConstAssignments getNonTransientAssignments() const;

    /*!
     * Returns all non-transient assignments in this set of assignments.
     */
    detail::ConstAssignments getNonTransientAssignments(int64_t assignmentLevel) const;

    /*!
     * Retrieves whether the set of assignments has at least one transient assignment.
     */
    bool hasTransientAssignment() const;

    /*!
     * Returns an iterator to the assignments.
     */
    detail::Assignments::iterator begin();

    /*!
     * Returns an iterator to the assignments.
     */
    detail::ConstAssignments::iterator begin() const;

    /*!
     * Returns an iterator past the end of the assignments.
     */
    detail::Assignments::iterator end();

    /*!
     * Returns an iterator past the end of the assignments.
     */
    detail::ConstAssignments::iterator end() const;

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /*!
     * Changes all variables in assignments based on the given mapping.
     */
    void changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping);

    /*!
     * Checks the assignments for linearity.
     */
    bool areLinear() const;

    friend std::ostream& operator<<(std::ostream& stream, OrderedAssignments const& assignments);

    OrderedAssignments clone() const;

    /*!
     * Checks whether this ordered assignment is in the correct order.
     */
    bool checkOrder() const;

   private:
    uint64_t isReadBeforeAssignment(LValue const& lValue, uint64_t assignmentNumber, uint64_t start = 0) const;
    uint64_t isWrittenBeforeAssignment(LValue const& LValue, uint64_t assignmentNumber, uint64_t start = 0) const;

    /*!
     * Gets the number of  assignments number with an assignment not higher than index.
     * @param index The index we are interested in
     * @return
     */
    uint64_t upperBound(int64_t index) const;

    static std::vector<std::shared_ptr<Assignment>>::const_iterator lowerBound(Assignment const& assignment,
                                                                               std::vector<std::shared_ptr<Assignment>> const& assignments);

    // The vectors to store the assignments. These need to be ordered at all times.
    std::vector<std::shared_ptr<Assignment>> allAssignments;
    std::vector<std::shared_ptr<Assignment>> transientAssignments;
    std::vector<std::shared_ptr<Assignment>> nonTransientAssignments;
};

}  // namespace jani
}  // namespace storm
