#pragma once

#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace analysis {

/*!
 * Class to collect constraints on parametric Markov chains.
 *
 * The collected constraints are stored as expressions in a Storm expression manager. In particular, this avoids a
 * dependency on a dedicated logical formula representation.
 *
 * Structurally equal constraints are deduplicated. Note that Storm's expressions hash and compare by identity of
 * their underlying node, so a container keyed by expressions themselves would not deduplicate them. We therefore
 * pair a structural hash (`storm::expressions::HashVisitor`) with `Expression::isSyntacticallyEqual`, which allows
 * looking up an existing constraint without scanning all of the collected ones. The constraints themselves are kept
 * in the order in which they are discovered, which is determined by the traversal of the model.
 */
class ConstraintCollector {
   public:
    /*!
     * The type of the collected constraints.
     */
    using ConstraintSet = std::vector<storm::expressions::Expression>;

   private:
    /*!
     * Hashes an expression by its structure instead of by the identity of its underlying node.
     */
    struct ExpressionStructuralHash {
        std::size_t operator()(storm::expressions::Expression const& expression) const;
    };

    /*!
     * Considers two expressions equal if they are syntactically equal. This is the counterpart of
     * ExpressionStructuralHash, so that syntactically equal expressions end up in the same bucket.
     */
    struct ExpressionSyntacticalEquality {
        bool operator()(storm::expressions::Expression const& first, storm::expressions::Expression const& second) const;
    };

    /*!
     * An index over a set of constraints, used to detect duplicates without scanning all of them.
     */
    using ConstraintIndex = std::unordered_set<storm::expressions::Expression, ExpressionStructuralHash, ExpressionSyntacticalEquality>;

    // The expression manager owning the collected constraints.
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;

    // A set of constraints that says that the DTMC actually has valid probability distributions in all states.
    ConstraintSet wellformedConstraintSet;

    // An index over the wellformed constraints. It mirrors wellformedConstraintSet.
    ConstraintIndex wellformedConstraintIndex;

    // A set of constraints that makes sure that the underlying graph of the model does not change depending
    // on the parameter values.
    ConstraintSet graphPreservingConstraintSet;

    // An index over the graph preserving constraints. It mirrors graphPreservingConstraintSet.
    ConstraintIndex graphPreservingConstraintIndex;

    // A set of variables
    std::set<storm::RationalFunctionVariable> variableSet;

    /*!
     * Builds the constraint that the given polynomial stands in the given relation to zero.
     *
     * Relations in which the polynomial occurs on the greater side are normalized by negating the polynomial,
     * so that e.g. `a >= 0` is built as `-a <= 0`. Without this normalization the same constraint would be
     * collected in several different shapes and could not be deduplicated.
     *
     * The polynomial arithmetic is performed on the carl polynomial rather than on the expression, so that carl
     * can put it into its canonical form. Otherwise, constraints that are equivalent would not be recognized as
     * such.
     *
     * @param polynomial The polynomial of the constraint.
     * @param relation The relation that the polynomial is to stand in to zero.
     * @return The corresponding constraint.
     */
    storm::expressions::Expression relateToZero(storm::RawPolynomial const& polynomial, storm::expressions::RelationType relation) const;

    /*!
     * Adds the given constraint to the set of well-formedness constraints, unless the set already contains a
     * structurally equal constraint.
     *
     * @param constraint The constraint to add.
     */
    void addWellformedConstraint(storm::expressions::Expression const& constraint);

    /*!
     * Adds the given constraint to the set of graph-preserving constraints, unless the set already contains a
     * structurally equal constraint.
     *
     * @param constraint The constraint to add.
     */
    void addGraphPreservingConstraint(storm::expressions::Expression const& constraint);

    /*!
     * Adds the constraints asserting that the given value is non-negative.
     *
     * @param value The value whose non-negativity is to be constrained.
     */
    void wellformedRequiresNonNegativeEntries(storm::RationalFunction const& value);

    /*!
     * Adds the constraints asserting that the given value is at most one. This is a no-op if the denominator of
     * the value is not constant, since the required sign reasoning is not expressible in the expression system
     * yet.
     *
     * @param value The value whose upper bound is to be constrained.
     */
    void wellformedRequiresAtMostOne(storm::RationalFunction const& value);

    /*!
     * Adds the constraint asserting that the given value is non-zero, which ensures that the underlying graph of
     * the model does not change with the parameter values.
     *
     * @param value The value that is constrained.
     */
    void graphPreservingRequiresNonZero(storm::RationalFunction const& value);

    /*!
     * Constructs the constraints for the given model.
     *
     * @param model The model for which to create the constraints.
     */
    void process(storm::models::sparse::Model<storm::RationalFunction> const& model);

   public:
    /*!
     * Constructs a constraint collector for the given model. The constraints are built and ready for
     * retrieval after the construction.
     *
     * @param model The model for which to create the constraints.
     */
    explicit ConstraintCollector(storm::models::sparse::Model<storm::RationalFunction> const& model);

    /*!
     * Returns the set of wellformed-ness constraints.
     *
     * @return The set of wellformed-ness constraints, keyed by their string representation.
     */
    ConstraintSet const& getWellformedConstraints() const;

    /*!
     * Returns the set of graph-preserving constraints.
     *
     * @return The set of graph-preserving constraints, keyed by their string representation.
     */
    ConstraintSet const& getGraphPreservingConstraints() const;

    /*!
     * Returns the set of variables in the model
     * @return
     */
    std::set<storm::RationalFunctionVariable> const& getVariables() const;

    /*!
     * Constructs the constraints for the given model by calling the process method.
     *
     * @param model The model for which to create the constraints.
     */
    void operator()(storm::models::sparse::Model<storm::RationalFunction> const& model);
};

}  // namespace analysis
}  // namespace storm
