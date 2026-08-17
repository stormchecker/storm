#pragma once

#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"

#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/helper/MDPModelCheckingHelperReturnType.h"
#include "storm/models/ModelRepresentation.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/DAProductBuilder.h"
#include "storm/transformer/SubsystemBuilder.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace helper {
namespace lexicographic {

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
class LexicographicModelCheckerHelper : public helper::SingleValueModelCheckerHelper<ValueType, storm::models::ModelRepresentation::Sparse> {
   public:
    typedef std::function<storm::storage::BitVector(storm::logic::Formula const&)> CheckFormulaCallback;
    typedef storm::transformer::SubsystemBuilderReturnType<ValueType, storm::models::sparse::StandardRewardModel<ValueType>> SubsystemReturnType;
    using StateType = storm::storage::sparse::state_type;
    using productModelType = typename storm::models::sparse::Mdp<ValueType>;

    // init
    LexicographicModelCheckerHelper(storm::logic::MultiObjectiveFormula const& formula, storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
        : _transitionMatrix(transitionMatrix), formula(formula) {}

    /*!
     * Returns the product of a model and the product-automaton of all sub-formulae of the multi-objective formula
     * @param model MDP
     * @param formulaChecker
     * @return product-model
     */
    std::pair<std::shared_ptr<storm::transformer::DAProduct<SparseModelType>>, std::vector<uint64_t>> getCompleteProductModel(
        SparseModelType const& model, CheckFormulaCallback const& formulaChecker);

    /*!
     * Given a product of an MDP and a automaton, returns the MECs and their corresponding Lex-Arrays
     * First: get MEC-decomposition
     * Second: for each MEC, run an algorithm to get Lex-arrays
     * @param productModel product of MDP and automaton
     * @param acceptanceConditions indication which Streett-pairs belong to which subformula
     * @return MECs, corresp. Lex-arrays
     */
    std::pair<storm::storage::MaximalEndComponentDecomposition<ValueType>, std::vector<std::vector<bool>>> getLexArrays(
        std::shared_ptr<storm::transformer::DAProduct<productModelType>> productModel, std::vector<uint64_t>& acceptanceConditions);

    /*!
     * Solves the reachability query for a lexicographic objective
     * In lexicographic order, each objective is solved for reachability, i.e. the MECs where the property can be fulfilled are the goal-states
     * The model is restricted to optimal actions concerning this reachability query
     * This is repeated for all objectives.
     * @param mecs MaximalEndcomponents in the product-model
     * @param mecLexArray corresponding Lex-arrays for each MEC
     * @param productModel the product of MDP and automaton
     * @param originalMdp the original MDP
     * @return
     */
    MDPSparseModelCheckingHelperReturnType<ValueType> lexReachability(storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs,
                                                                      std::vector<std::vector<bool>> const& mecLexArray,
                                                                      std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> const& productModel,
                                                                      SparseModelType const& originalMdp);

   private:
    static std::map<std::string, storm::storage::BitVector> computeApSets(std::map<std::string, std::shared_ptr<storm::logic::Formula const>> const& extracted,
                                                                          CheckFormulaCallback const& formulaChecker);

    /*!
     * parses an acceptance condition and returns the set of contained Streett-pairs
     * it is called recursively
     * A Streett-pair is assumed to be an OR-condition (Fin | Inf)
     * @param current acceptance condition
     * @return
     */
    std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> getStreettPairs(
        storm::automata::AcceptanceCondition::acceptance_expr::ptr const& current);

    /*!
     * Checks whether a Streett-condition can be fulfilled in an MEC
     * @param mec MEC
     * @param acceptancePairs list of Streett-pairs that create the Streett-condition
     * @param acceptance original acceptance condition of the automaton
     * @param model copy of the product-model
     * @return whether the condition can be fulfilled or not
     */
    bool isAcceptingStreettConditions(storm::storage::MaximalEndComponent const& scc,
                                      std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> const& acceptancePairs,
                                      storm::automata::AcceptanceCondition::ptr const& acceptance, productModelType const& model);

    /*!
     * For a given objective, iterates over the MECs and finds the corresponding sink state
     * @param mecs all MECs in the model
     * @param mecLexArrays the corresponding lex-arrays
     * @param oldToNewStateMapping mapping of original states and compressed states
     * @param condition the condition to be checked
     * @param numStatesTotal the number of states in total in the compressed model
     * @param mecToStateMapping mapping of the MECs to their corresponding sink state
     * @param compressedToReducedMapping mapping of the compressed model to the reduced to optimal
     * @return set of "good" states for the given condition
     */
    storm::storage::BitVector getGoodStates(storm::storage::MaximalEndComponentDecomposition<ValueType> const& bcc,
                                            std::vector<std::vector<bool>> const& bccLexArray, std::vector<uint64_t> const& oldToNewStateMapping,
                                            uint64_t const& condition, uint64_t const numStates, std::vector<uint64_t> const& compressedToReducedMapping,
                                            std::map<uint64_t, uint64_t> const& bccToStStateMapping);

    /*!
     * Solves the reachability-query for a given set of goal-states and initial-states
     */
    MDPSparseModelCheckingHelperReturnType<ValueType> solveOneReachability(std::vector<uint64_t>& newInitalStates, storm::storage::BitVector const& psiStates,
                                                                           storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                           SparseModelType const& originalMdp,
                                                                           std::vector<uint64_t> const& compressedToReducedMapping,
                                                                           std::vector<uint64_t> const& oldToNewStateMapping);

    /*!
     * Reduces the model to actions that are optimal for the given strategy.
     * @param transitionMatrix current transition matrix
     * @param reachabilityResult result of the reachability query, that contains (i) the reachability value for each state, and (ii) the optimal scheduler
     * @return
     */
    SubsystemReturnType getReducedSubsystem(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                            MDPSparseModelCheckingHelperReturnType<ValueType> const& reachabilityResult,
                                            std::vector<uint64_t> const& newInitalStates, storm::storage::BitVector const& goodStates);

    /*!
     * add a new sink-state for each MEC
     */
    std::pair<storm::storage::SparseMatrix<ValueType>, std::map<uint64_t, uint64_t>> addSinkStates(
        storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs,
        std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> const& productModel);

    storm::storage::SparseMatrix<ValueType> const& _transitionMatrix;
    storm::logic::MultiObjectiveFormula const& formula;
};

}  // namespace lexicographic
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
