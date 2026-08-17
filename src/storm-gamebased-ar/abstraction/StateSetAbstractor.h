#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <set>

#include "storm/storage/dd/DdType.h"

#include "storm/solver/SmtSolver.h"
#include "storm/utility/solver.h"

#include "storm-gamebased-ar/abstraction/LocalExpressionInformation.h"

namespace storm {
namespace utility {
namespace solver {
class SmtSolverFactory;
}
}  // namespace utility

namespace dd {
template<storm::dd::DdType DdType>
class Bdd;

template<storm::dd::DdType DdType, typename ValueType>
class Add;
}  // namespace dd
}  // namespace storm

namespace storm::gbar {
namespace abstraction {
template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType, typename ValueType>
class StateSetAbstractor {
   public:
    StateSetAbstractor(StateSetAbstractor const& other) = default;
    StateSetAbstractor& operator=(StateSetAbstractor const& other) = default;

    StateSetAbstractor(StateSetAbstractor&& other) = default;
    StateSetAbstractor& operator=(StateSetAbstractor&& other) = default;

    /*!
     * Creates a state set abstractor.
     *
     * @param abstractionInformation An object storing information about the abstraction such as predicates and BDDs.
     * @param statePredicates A set of predicates that have to hold in the concrete states this abstractor is
     * supposed to abstract.
     * @param smtSolverFactory A factory that can create new SMT solvers.
     */
    StateSetAbstractor(AbstractionInformation<DdType>& abstractionInformation, std::vector<storm::expressions::Expression> const& statePredicates,
                       std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory =
                           std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    /*!
     * Refines the abstractor by making the given predicates new abstract predicates.
     *
     * @param newPredicateIndices The indices of the new predicates.
     */
    void refine(std::vector<uint_fast64_t> const& newPredicateIndices);

    /*!
     * Constrains the abstract states with the given expression.
     * Note that all constaints must be added before the abstractor is used to retrieve the abstract states.
     *
     * @param constraint The constraint to add.
     */
    void constrain(storm::expressions::Expression const& constraint);

    /*!
     * Constraints the abstract states with the given BDD.
     *
     * @param newConstraint The BDD used as the constraint.
     */
    void constrain(storm::dd::Bdd<DdType> const& newConstraint);

    /*!
     * Retrieves the set of abstract states matching all predicates added to this abstractor.
     *
     * @return The set of matching abstract states in the form of a BDD
     */
    storm::dd::Bdd<DdType> getAbstractStates();

   private:
    /*!
     * Creates decision variables and the corresponding constraints for the missing predicates.
     *
     * @param newRelevantPredicateIndices The set of all relevant predicate indices.
     */
    void addMissingPredicates(std::set<uint_fast64_t> const& newRelevantPredicateIndices);

    /*!
     * Adds the current constraint BDD to the solver.
     */
    void pushConstraintBdd();

    /*!
     * Removes the current constraint BDD (if any) from the solver.
     */
    void popConstraintBdd();

    /*!
     * Recomputes the cached BDD. This needs to be triggered if any relevant predicates change.
     */
    void recomputeCachedBdd();

    /*!
     * Translates the given model to a state DD.
     *
     * @param model The model to translate.
     * @return The state encoded as a DD.
     */
    storm::dd::Bdd<DdType> getStateBdd(storm::solver::SmtSolver::ModelReference const& model) const;

    /*!
     * Retrieves the abstraction information.
     *
     * @return The abstraction information.
     */
    AbstractionInformation<DdType>& getAbstractionInformation();

    /*!
     * Retrieves the abstraction information.
     *
     * @return The abstraction information.
     */
    AbstractionInformation<DdType> const& getAbstractionInformation() const;

    // The SMT solver used for abstracting the set of states.
    std::unique_ptr<storm::solver::SmtSolver> smtSolver;

    // The abstraction-related information.
    std::reference_wrapper<AbstractionInformation<DdType>> abstractionInformation;

    // The local expression-related information.
    LocalExpressionInformation<DdType> localExpressionInformation;

    // The set of relevant predicates and the corresponding decision variables.
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> relevantPredicatesAndVariables;

    // The set of all variables appearing in the concrete predicates.
    std::set<storm::expressions::Variable> concretePredicateVariables;

    // The set of all decision variables over which to perform the all-sat enumeration.
    std::vector<storm::expressions::Variable> decisionVariables;

    // A flag indicating whether the cached BDD needs recomputation.
    bool forceRecomputation;

    // The cached BDD representing the abstraction. This variable is written to in refinement steps (if work
    // needed to be done).
    storm::dd::Bdd<DdType> cachedBdd;

    // This BDD currently constrains the search for solutions.
    storm::dd::Bdd<DdType> constraint;
};
}  // namespace abstraction
}  // namespace storm::gbar
