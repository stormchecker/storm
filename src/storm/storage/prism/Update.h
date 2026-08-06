#pragma once

#include <vector>

#include "storm/storage/prism/Assignment.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace prism {
class Update : public LocatedInformation {
   public:
    using ExpressionPair = std::pair<storm::expressions::Expression, storm::expressions::Expression>;

    /*!
     * Creates an update with the given expression specifying the likelihood and assignments.
     *
     * @param globalIndex The global index of the update.
     * @param likelihoodExpression An expression specifying the likelihood of this update.
     * @param assignments A assignments to variables.
     * @param filename The filename in which the update is defined.
     * @param lineNumber The line number in which the update is defined.
     */
    Update(uint_fast64_t globalIndex, storm::expressions::Expression const& likelihoodExpression, std::vector<storm::prism::Assignment> const& assignments,
           std::string const& filename = "", uint_fast64_t lineNumber = 0);
    /*!
     * Creates an update with the given expression specifying the likelihood (possibly as an interval) and assignments.
     * The likelihood is assumed to be an interval iff likelihoodExpressionInterval.second.isInitialized() holds.
     *
     * @param globalIndex The global index of the update.
     * @param likelihoodExpression expressions specifying the likelihood of this update.
     * @param assignments A assignments to variables.
     * @param filename The filename in which the update is defined.
     * @param lineNumber The line number in which the update is defined.
     */
    Update(uint_fast64_t globalIndex, ExpressionPair const& likelihoodExpressionInterval, std::vector<storm::prism::Assignment> const& assignments,
           std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Update() = default;
    Update(Update const& other) = default;
    Update& operator=(Update const& other) = default;
    Update(Update&& other) = default;
    Update& operator=(Update&& other) = default;

    bool isLikelihoodInterval() const;

    /*!
     * Retrieves the expression for the likelihood of this update.
     *
     * @return The expression for the likelihood of this update.
     * @pre the likelihood is not an interval
     */
    storm::expressions::Expression const& getLikelihoodExpression() const;

    /*!
     * Retrieves the two expression for the interval likelihood of this update.
     *
     * @return The expressions representing the likelihood interval [first, second].
     * @pre the likelihood is an interval
     */
    ExpressionPair const& getLikelihoodExpressionInterval() const;

    /*!
     * Retrieves the number of assignments associated with this update.
     *
     * @return The number of assignments associated with this update.
     */
    std::size_t getNumberOfAssignments() const;

    /*!
     * Retrieves a reference to the map of variable names to their respective assignments.
     *
     * @return A reference to the map of variable names to their respective assignments.
     */
    std::vector<storm::prism::Assignment> const& getAssignments() const;

    /*!
     * Retrieves a reference to the map of variable names to their respective assignments.
     *
     * @return A reference to the map of variable names to their respective assignments.
     */
    std::vector<storm::prism::Assignment>& getAssignments();

    /*!
     * Retrieves a reference to the assignment for the variable with the given name.
     *
     * @return A reference to the assignment for the variable with the given name.
     */
    storm::prism::Assignment const& getAssignment(std::string const& variableName) const;

    /*!
     * Creates a mapping representation of this update.
     *
     * @return A mapping from variables to expressions.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getAsVariableToExpressionMap() const;

    /*!
     * Retrieves the global index of the update, that is, a unique index over all modules.
     *
     * @return The global index of the update.
     */
    uint_fast64_t getGlobalIndex() const;

    /*!
     * Substitutes all identifiers in the update according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting update.
     */
    Update substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    /*!
     * Removes all assignments which do not change the variable.
     *
     * @return The resulting update.
     */
    Update removeIdentityAssignments() const;

    Update substituteNonStandardPredicates() const;

    /*!
     * Simplifies the update in various ways (also removes identity assignments)
     */
    Update simplify() const;

    friend std::ostream& operator<<(std::ostream& stream, Update const& assignment);

   private:
    /*!
     * Creates the internal mapping of variables to their assignments.
     */
    void createAssignmentMapping();

    // Expressions specifying the likelihood of taking this update.
    // Either both expressions are initialized (representing an interval likelihood [first, second] for interval models)
    // or only the first one is initialized (standard, non-interval interpretation)
    ExpressionPair likelihoodExpressions;

    // The assignments of this update.
    std::vector<storm::prism::Assignment> assignments;

    // A mapping from variable names to their assignments.
    std::map<std::string, uint_fast64_t> variableToAssignmentIndexMap;

    // The global index of the update.
    uint_fast64_t globalIndex;
};
}  // namespace prism
}  // namespace storm
