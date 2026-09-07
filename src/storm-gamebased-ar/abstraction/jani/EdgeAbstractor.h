#pragma once

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "storm-gamebased-ar/abstraction/GameBddResult.h"
#include "storm-gamebased-ar/abstraction/LocalExpressionInformation.h"
#include "storm-gamebased-ar/abstraction/StateSetAbstractor.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"

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

namespace jani {
// Forward-declare concrete edge and assignment classes.
class Edge;
class Assignment;
class OrderedAssignments;
}  // namespace jani
}  // namespace storm

namespace storm::gbar {
namespace abstraction {
template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType>
struct BottomStateResult;

namespace jani {
template<storm::dd::DdType DdType, typename ValueType>
class EdgeAbstractor {
   public:
    /*!
     * Constructs an abstract edge from the given command and the initial predicates.
     *
     * @param edgeId The ID to assign to the edge.
     * @param edge The concrete edge for which to build the abstraction.
     * @param abstractionInformation An object holding information about the abstraction such as predicates and BDDs.
     * @param smtSolverFactory A factory that is to be used for creating new SMT solvers.
     * @param useDecomposition A flag indicating whether to use an edge decomposition during abstraction.
     */
    EdgeAbstractor(uint64_t edgeId, storm::jani::Edge const& edge, AbstractionInformation<DdType>& abstractionInformation,
                   std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory, bool useDecomposition, bool addPredicatesForValidBlocks,
                   bool debug);

    /*!
     * Refines the abstract edge with the given predicates.
     *
     * @param predicates The new predicates.
     */
    void refine(std::vector<uint_fast64_t> const& predicates);

    /*!
     * Retrieves the guard of this edge.
     */
    storm::expressions::Expression const& getGuard() const;

    /*!
     * Retrieves the number of updates of this edge.
     */
    uint64_t getNumberOfUpdates(uint64_t player1Choice) const;

    /*!
     * Retrieves a mapping from variables to expressions that define their updates wrt. to the given
     * auxiliary choice.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getVariableUpdates(uint64_t auxiliaryChoice) const;

    /*!
     * Retrieves the assigned variables.
     */
    std::set<storm::expressions::Variable> const& getAssignedVariables() const;

    /*!
     * Computes the abstraction of the edge wrt. to the current set of predicates.
     *
     * @return The abstraction of the edge in the form of a BDD together with the number of DD variables
     * used to encode the choices of player 2.
     */
    GameBddResult<DdType> abstract();

    /*!
     * Retrieves the transitions to bottom states of this edge.
     *
     * @param reachableStates A BDD representing the reachable states.
     * @param numberOfPlayer2Variables The number of variables used to encode the choices of player 2.
     * @return The bottom state transitions in the form of a BDD.
     */
    BottomStateResult<DdType> getBottomStateTransitions(
        storm::dd::Bdd<DdType> const& reachableStates, uint_fast64_t numberOfPlayer2Variables,
        boost::optional<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& locationVariables);

    /*!
     * Retrieves an ADD that maps the encoding of the edge, source/target locations and its updates to their probabilities.
     *
     * @param locationVariables If provided, the location variables are used to encode the source/destination locations of the edge
     * and its destinations.
     * @return The decorator ADD.
     */
    storm::dd::Add<DdType, ValueType> getEdgeDecoratorAdd(
        boost::optional<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& locationVariables) const;

    /*!
     * Retrieves the concrete edge that is abstracted by this abstract edge.
     *
     * @return The concrete edge.
     */
    storm::jani::Edge const& getConcreteEdge() const;

    void notifyGuardIsPredicate();

   private:
    /*!
     * Determines the relevant predicates for source as well as successor states wrt. to the given assignments
     * (that, for example, form an update).
     *
     * @param assignments The assignments that are to be considered.
     * @return A pair whose first component represents the relevant source predicates and whose second
     * component represents the relevant successor state predicates.
     */
    std::pair<std::set<uint_fast64_t>, std::set<uint_fast64_t>> computeRelevantPredicates(storm::jani::OrderedAssignments const& assignments) const;

    /*!
     * Determines the relevant predicates for source as well as successor states.
     *
     * @return A pair whose first component represents the relevant source predicates and whose second
     * component represents the relevant successor state predicates.
     */
    std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> computeRelevantPredicates() const;

    /*!
     * Checks whether the relevant predicates changed.
     *
     * @param newRelevantPredicates The new relevant predicates.
     */
    bool relevantPredicatesChanged(std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> const& newRelevantPredicates) const;

    /*!
     * Takes the new relevant predicates and creates the appropriate variables and assertions for the ones
     * that are currently missing.
     *
     * @param newRelevantPredicates The new relevant predicates.
     */
    void addMissingPredicates(std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> const& newRelevantPredicates);

    /*!
     * Translates the given model to a source state DD.
     *
     * @param model The model to translate.
     * @return The source state encoded as a DD.
     */
    storm::dd::Bdd<DdType> getSourceStateBdd(storm::solver::SmtSolver::ModelReference const& model,
                                             std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> const& variablePredicates) const;

    /*!
     * Translates the given model to a distribution over successor states.
     *
     * @param model The model to translate.
     * @return The source state encoded as a DD.
     */
    storm::dd::Bdd<DdType> getDistributionBdd(storm::solver::SmtSolver::ModelReference const& model,
                                              std::vector<std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>>> const& variablePredicates) const;

    /*!
     * Recomputes the cached BDD. This needs to be triggered if any relevant predicates change.
     */
    void recomputeCachedBdd();

    /*!
     * Recomputes the cached BDD without the decomposition\.
     */
    void recomputeCachedBddWithoutDecomposition();

    /*!
     * Recomputes the cached BDD using the decomposition.
     */
    void recomputeCachedBddWithDecomposition();

    /*!
     * Computes the missing state identities for the destinations.
     *
     * @return A BDD that represents the state identities for predicates that are irrelevant for the
     * successor states.
     */
    storm::dd::Bdd<DdType> computeMissingDestinationIdentities() const;

    /*!
     * Retrieves the abstraction information object.
     *
     * @return The abstraction information object.
     */
    AbstractionInformation<DdType> const& getAbstractionInformation() const;

    /*!
     * Retrieves the abstraction information object.
     *
     * @return The abstraction information object.
     */
    AbstractionInformation<DdType>& getAbstractionInformation();

    // An SMT responsible for this abstract command.
    std::unique_ptr<storm::solver::SmtSolver> smtSolver;

    // The abstraction-related information.
    std::reference_wrapper<AbstractionInformation<DdType>> abstractionInformation;

    // The ID of the edge.
    uint64_t edgeId;

    // The concrete edge this abstract command refers to.
    std::reference_wrapper<storm::jani::Edge const> edge;

    // The variables to which this edge assigns expressions.
    std::set<storm::expressions::Variable> assignedVariables;

    // The local expression-related information.
    LocalExpressionInformation<DdType> localExpressionInformation;

    // The evaluator used to translate the probability expressions.
    storm::expressions::ExpressionEvaluator<ValueType> evaluator;

    // The currently relevant source/successor predicates and the corresponding variables.
    std::pair<std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>>,
              std::vector<std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>>>>
        relevantPredicatesAndVariables;

    // The set of all relevant predicates.
    std::set<uint64_t> allRelevantPredicates;

    // The most recent result of a call to computeDd. If nothing has changed regarding the relevant
    // predicates, this result may be reused.
    GameBddResult<DdType> cachedDd;

    // All relevant decision variables over which to perform AllSat.
    std::vector<storm::expressions::Variable> decisionVariables;

    // A flag indicating whether to use the decomposition when abstracting.
    bool useDecomposition;

    // Whether or not to add predicates indirectly related to assignment variables to relevant source predicates.
    bool addPredicatesForValidBlocks;

    // A flag indicating whether the computation of bottom states can be skipped (for example, if the bottom
    // states become empty at some point).
    bool skipBottomStates;

    // A flag remembering whether we need to force recomputation of the BDD.
    bool forceRecomputation;

    // The abstract guard of the edge. This is only used if the guard is not a predicate, because it can
    // then be used to constrain the bottom state abstractor.
    storm::dd::Bdd<DdType> abstractGuard;

    // A state-set abstractor used to determine the bottom states if not all guards were added.
    StateSetAbstractor<DdType, ValueType> bottomStateAbstractor;

    // A flag that indicates whether or not debug mode is enabled.
    bool debug;
};
}  // namespace jani
}  // namespace abstraction
}  // namespace storm::gbar
