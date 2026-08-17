#pragma once

#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"
#include "storm/modelchecker/helper/ltl/internal/SparseLTLSchedulerHelper.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/DAProductBuilder.h"

namespace storm {

class Environment;

namespace logic {
class Formula;
class PathFormula;
}  // namespace logic
namespace automata {
class DeterministicAutomaton;
}

namespace modelchecker {
namespace helper {

/*!
 * Helper class for LTL model checking
 * @tparam ValueType the type a value can have
 * @tparam Nondeterministic A flag indicating if there is nondeterminism in the Model (MDP)
 */
template<typename ValueType, bool Nondeterministic>
class SparseLTLHelper : public SingleValueModelCheckerHelper<ValueType, storm::models::ModelRepresentation::Sparse> {
   public:
    typedef std::function<storm::storage::BitVector(storm::logic::Formula const&)> CheckFormulaCallback;

    /*!
     * The type of the product model (DTMC or MDP) that is used during the computation.
     */
    using productModelType = typename std::conditional<Nondeterministic, storm::models::sparse::Mdp<ValueType>, storm::models::sparse::Dtmc<ValueType>>::type;

    /*!
     * Initializes the helper for a discrete time model (i.e. DTMC, MDP)
     * @param transitionMatrix the transition matrix of the model
     */
    SparseLTLHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    /*!
     * @pre before calling this, a computation call should have been performed during which scheduler production was enabled.
     * @param model the model
     * @return a new scheduler containing optimal choices for each state that yield the long run average values of the most recent call.
     */
    storm::storage::Scheduler<ValueType> extractScheduler(storm::models::sparse::Model<ValueType> const& model);

    /*!
     * Computes the LTL probabilities
     * @param formula the LTL formula (allowing PCTL* like nesting)
     * @param formulaChecker lambda that evaluates sub-formulas checks the provided formula and returns the set of states in which the formula holds<
     * @return a value for each state
     */
    std::vector<ValueType> computeLTLProbabilities(Environment const& env, storm::logic::PathFormula const& formula,
                                                   CheckFormulaCallback const& formulaChecker);

    /*!
     * Computes the states that are satisfying the AP.
     * @param extracted mapping from Ap to formula
     * @param formulaChecker lambda that checks the provided formula and returns the set of states in which the formula holds
     * @return mapping from AP to satisfaction sets
     */
    static std::map<std::string, storm::storage::BitVector> computeApSets(std::map<std::string, std::shared_ptr<storm::logic::Formula const>> const& extracted,
                                                                          CheckFormulaCallback const& formulaChecker);

    /*!
     * Computes the (maximizing) probabilities for the constructed DA product
     * @param da the DA to build the product with
     * @param apSatSets the atomic propositions and satisfaction sets
     * @return a value for each state
     */
    std::vector<ValueType> computeDAProductProbabilities(Environment const& env, storm::automata::DeterministicAutomaton const& da,
                                                         std::map<std::string, storm::storage::BitVector>& apSatSets);

    /*!
     * Computes the LTL probabilities
     * @param formula the LTL formula (without PCTL*-like nesting)
     * @param apSatSets a mapping from all atomic propositions occuring in the formula to the corresponding satisfaction set
     * @return a value for each state
     */
    std::vector<ValueType> computeLTLProbabilities(Environment const& env, storm::logic::PathFormula const& formula,
                                                   std::map<std::string, storm::storage::BitVector>& apSatSets);

   private:
    /*!
     * Computes a set S of states that admit a probability 1 strategy of satisfying the given acceptance condition (in DNF).
     * More precisely, let
     *   accEC be the set of states that are contained in end components that satisfy the acceptance condition
     *  and let
     *   P1acc be the set of states that satisfy Pmax=1[ F accEC ].
     * This function then computes a set that contains accEC and is contained by P1acc.
     * However, if the acceptance condition consists of 'true', the whole state space can be returned.
     * @param acceptance the acceptance condition (in DNF)
     * @param transitionMatrix the transition matrix of the model
     * @param backwardTransitions the reversed transition relation
     */
    storm::storage::BitVector computeAcceptingECs(automata::AcceptanceCondition const& acceptance,
                                                  storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                  typename transformer::DAProduct<productModelType>::ptr product);

    /*!
     * Computes a set S of states that are contained in BSCCs that satisfy the given acceptance conditon.
     * @param acceptance the acceptance condition
     * @param transitionMatrix the transition matrix of the model
     */
    storm::storage::BitVector computeAcceptingBCCs(automata::AcceptanceCondition const& acceptance,
                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    storm::storage::SparseMatrix<ValueType> const& _transitionMatrix;

    boost::optional<storm::modelchecker::helper::internal::SparseLTLSchedulerHelper<ValueType, Nondeterministic>> _schedulerHelper;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm