#pragma once

#include <limits>
#include <set>

#include "storm/models/sparse/DeterministicModel.h"
#include "storm/models/sparse/NondeterministicModel.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/StronglyConnectedComponent.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace storage {
class BitVector;
template<typename VT>
class SparseMatrix;
}  // namespace storage

namespace models {

namespace symbolic {
template<storm::dd::DdType Type, typename ValueType>
class Model;

template<storm::dd::DdType Type, typename ValueType>
class DeterministicModel;

template<storm::dd::DdType Type, typename ValueType>
class NondeterministicModel;

template<storm::dd::DdType Type, typename ValueType>
class StochasticTwoPlayerGame;
}  // namespace symbolic

}  // namespace models

namespace dd {
template<storm::dd::DdType Type>
class Bdd;

template<storm::dd::DdType Type, typename ValueType>
class Add;

}  // namespace dd

namespace storage {
class ExplicitGameStrategyPair;
}

namespace utility {
namespace graph {

/*!
 * Computes the states reachable in one step from the states indicated by the bitvector.
 * Assumes that no zero entries exist in the transition matrix.
 */
template<typename T>
storm::storage::BitVector getReachableOneStep(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates);

/*!
 * Performs a forward depth-first search through the underlying graph structure to identify the states that
 * are reachable from the given set only passing through a constrained set of states until some target
 * have been reached.
 * If an initial state or a (constrained-reachable) target state is not in the constrained set, it will be added to the reachable states but not explored.
 *
 * @param transitionMatrix The transition relation of the graph structure to search.
 * @param initialStates The set of states from which to start the search.
 * @param constraintStates The set of states that must not be left.
 * @param targetStates The target states that may not be passed.
 * @param useStepBound A flag that indicates whether or not to use the given number of maximal steps for the search.
 * @param maximalSteps The maximal number of steps to reach the psi states.
 * @param choiceFilter If given, only choices for which the bitvector is true are considered.
 */
template<typename T>
storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                                             storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                                             bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                             boost::optional<storm::storage::BitVector> const& choiceFilter = boost::none);

/*!
 * Retrieves a set of states that covers als BSCCs of the system in the sense that for every BSCC exactly
 * one state is included in the cover.
 *
 * @param transitionMatrix The transition relation of the graph structure.
 */
template<typename T>
storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<T> const& transitionMatrix);

/*!
 * Returns true if the graph represented by the given matrix has a cycle
 * @param transitionMatrix
 * @param subsystem if given, only states in the subsystem are considered for the check.
 */
template<typename T>
bool hasCycle(storm::storage::SparseMatrix<T> const& transitionMatrix, boost::optional<storm::storage::BitVector> const& subsystem = boost::none);

/*!
 * Checks whether there is an End Component that
 * 1. contains at least one of the specified choices and
 * 2. only contains states given by the specified subsystem.
 *
 * @param transitionMatrix the transition matrix
 * @param backwardTransitions The reversed transition relation of the graph structure to search
 * @param subsystem the subsystem which we consider
 * @param choices the choices which are to be checked
 */
template<typename T>
bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::SparseMatrix<T> const& backwardTransitions,
                               storm::storage::BitVector const& subsystem, storm::storage::BitVector const& choices);

/*!
 * Performs a breadth-first search through the underlying graph structure to compute the distance from all
 * states to the starting states of the search.
 *
 * @param transitionMatrix The transition relation of the graph structure to search.
 * @param initialStates The set of states from which to start the search.
 * @param subsystem The subsystem to consider.
 * @return The distances of each state to the initial states of the sarch.
 */
template<typename T>
std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                                        boost::optional<storm::storage::BitVector> const& subsystem = boost::none);

/*!
 * Performs a backward depth-first search trough the underlying graph structure
 * of the given model to determine which states of the model have a positive probability
 * of satisfying phi until psi. The resulting states are written to the given bit vector.
 *
 * @param backwardTransitions The reversed transition relation of the graph structure to search.
 * @param phiStates A bit vector of all states satisfying phi.
 * @param psiStates A bit vector of all states satisfying psi.
 * @param useStepBound A flag that indicates whether or not to use the given number of maximal steps for the search.
 * @param maximalSteps The maximal number of steps to reach the psi states.
 * @return A bit vector with all indices of states that have a probability greater than 0.
 */
template<typename T>
storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                              storm::storage::BitVector const& psiStates, bool useStepBound = false, uint_fast64_t maximalSteps = 0);

/*!
 * Computes the set of states of the given model for which all paths lead to
 * the given set of target states and only visit states from the filter set
 * before. In order to do this, it uses the given set of states that
 * characterizes the states that possess at least one path to a target state.
 * The results are written to the given bit vector.
 *
 * @param backwardTransitions The reversed transition relation of the graph structure to search.
 * @param phiStates A bit vector of all states satisfying phi.
 * @param psiStates A bit vector of all states satisfying psi.
 * @param statesWithProbabilityGreater0 A reference to a bit vector of states that possess a positive
 * probability mass of satisfying phi until psi.
 * @return A bit vector with all indices of states that have a probability greater than 1.
 */
template<typename T>
storm::storage::BitVector performProb1(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                       storm::storage::BitVector const& psiStates, storm::storage::BitVector const& statesWithProbabilityGreater0);

/*!
 * Computes the set of states of the given model for which all paths lead to
 * the given set of target states and only visit states from the filter set
 * before. In order to do this, it uses the given set of states that
 * characterizes the states that possess at least one path to a target state.
 * The results are written to the given bit vector.
 *
 * @param backwardTransitions The reversed transition relation of the graph structure to search.
 * @param phiStates A bit vector of all states satisfying phi.
 * @param psiStates A bit vector of all states satisfying psi.
 * @return A bit vector with all indices of states that have a probability greater than 1.
 */
template<typename T>
storm::storage::BitVector performProb1(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                       storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi until psi in a
 * deterministic model.
 *
 * @param model The model whose graph structure to search.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A pair of bit vectors such that the first bit vector stores the indices of all states
 * with probability 0 and the second stores all indices of states with probability 1.
 */
template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::models::sparse::DeterministicModel<T> const& model,
                                                                              storm::storage::BitVector const& phiStates,
                                                                              storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi until psi in a
 * deterministic model.
 *
 * @param backwardTransitions The backward transitions of the model whose graph structure to search.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A pair of bit vectors such that the first bit vector stores the indices of all states
 * with probability 0 and the second stores all indices of states with probability 1.
 */
template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                              storm::storage::BitVector const& phiStates,
                                                                              storm::storage::BitVector const& psiStates);

/*!
 * Computes the set of states that has a positive probability of reaching psi states after only passing
 * through phi states before.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param stepBound If given, this number indicates the maximal amount of steps allowed.
 * @return All states with positive probability.
 */
template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProbGreater0(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                         storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates,
                                         boost::optional<uint_fast64_t> const& stepBound = boost::optional<uint_fast64_t>());
/*!
 * Computes the set of states that have a probability of one of reaching psi states after only passing
 * through phi states before.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param statesWithProbabilityGreater0 The set of states with a positive probability of satisfying phi
 * until psi as a BDD.
 * @return All states with probability 1.
 */
template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                  storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates,
                                  storm::dd::Bdd<Type> const& statesWithProbabilityGreater0);

/*!
 * Computes the set of states that have a probability of one of reaching psi states after only passing
 * through phi states before.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return All states with probability 1.
 */
template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                  storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi until psi in a
 * deterministic model.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param phiStates The BDD containing all  phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return A pair of BDDs that represent all states with probability 0 and 1, respectively.
 */
template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01(storm::models::symbolic::DeterministicModel<Type, ValueType> const& model,
                                                                    storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi until psi in a
 * deterministic model.
 *
 * @param model The (symbolic) model for which to compute the set of states. This is used for retrieving the
 * manager and information about the meta variables.
 * @param phiStates The BDD containing all  phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return A pair of BDDs that represent all states with probability 0 and 1, respectively.
 */
template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01(storm::models::symbolic::Model<Type, ValueType> const& model,
                                                                    storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                    storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes a scheduler for the given states that chooses an action that stays completely in the very same set.
 * Note that this assumes that there is a legal choice for each of the states.
 *
 * @param states The set of states for which to compute the scheduler that stays in this very set.
 * @param transitionMatrix The transition matrix.
 * @param scheduler The resulting scheduler. The scheduler is only set at the given states.
 * @param rowFilter If given, the returned scheduler will only pick choices such that rowFilter is true for the corresponding matrixrow.
 */
template<typename T, typename SchedulerValueType>
void computeSchedulerStayingInStates(storm::storage::BitVector const& states, storm::storage::SparseMatrix<T> const& transitionMatrix,
                                     storm::storage::Scheduler<SchedulerValueType>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

/*!
 * Computes a scheduler for the given states that chooses an action that has at least one successor in the
 * given set of states. Note that this assumes that there is a legal choice for each of the states.
 *
 * @param states The set of states for which to compute the scheduler that chooses an action with a successor
 * in this very set.
 * @param transitionMatrix The transition matrix.
 * @param scheduler The resulting scheduler. The scheduler is only set at the given states.
 */
template<typename T>
void computeSchedulerWithOneSuccessorInStates(storm::storage::BitVector const& states, storm::storage::SparseMatrix<T> const& transitionMatrix,
                                              storm::storage::Scheduler<T>& scheduler);

/*!
 * Computes a scheduler for the ProbGreater0E-States such that in the induced system the given psiStates are reachable via phiStates
 *
 * @param transitionMatrix The transition matrix of the system.
 * @param backwardTransitions The reversed transition relation.
 * @param phiStates The set of states satisfying phi.
 * @param psiStates The set of states satisfying psi.
 * @param rowFilter If given, the returned scheduler will only pick choices such that rowFilter is true for the corresponding matrixrow.
 * @param scheduler The resulting scheduler for the ProbGreater0E-States. The scheduler is not set at prob0A states.
 *
 * @note No choice is defined for ProbGreater0E-States if all the probGreater0-choices violate the row filter.
 *       This also holds for states that only reach psi via such states.
 */
template<typename T, typename SchedulerValueType>
void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::SparseMatrix<T> const& backwardTransitions,
                                   storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                   storm::storage::Scheduler<SchedulerValueType>& scheduler,
                                   boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

/*!
 * Computes a scheduler for the given states that have a scheduler that has a reward infinity.
 *
 * @param rewInfStates The states that have a scheduler achieving reward infinity.
 * @param transitionMatrix The transition matrix of the system.
 * @param backwardTransitions The reversed transition relation.
 * @param scheduler The resulting scheduler for the rewInf States. The scheduler is not set at other states.
 */
template<typename T, typename SchedulerValueType>
void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::Scheduler<SchedulerValueType>& scheduler);

/*!
 * Computes a scheduler for the given states that have a scheduler that has a probability 0.
 *
 * @param prob0EStates The states that have a scheduler achieving probablity 0.
 * @param transitionMatrix The transition matrix of the system.
 * @param scheduler The resulting scheduler for the prob0EStates States. The scheduler is not set at probGreater0A states.
 */
template<typename T, typename SchedulerValueType>
void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::Scheduler<SchedulerValueType>& scheduler);

/*!
 * Computes a scheduler for the given prob1EStates such that in the induced system the given psiStates are reached with probability 1.
 *
 * @param prob1EStates The states that have a scheduler achieving probablity 1.
 * @param transitionMatrix The transition matrix of the system.
 * @param backwardTransitions The reversed transition relation.
 * @param phiStates The set of states satisfying phi.
 * @param psiStates The set of states satisfying psi.
 * @param scheduler The resulting scheduler for the prob1EStates. The scheduler is not set at the remaining states.
 * @param rowFilter If given, only scheduler choices within this filter are taken. This filter is ignored for the psiStates.
 */
template<typename T, typename SchedulerValueType>
void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                            storm::storage::BitVector const& psiStates, storm::storage::Scheduler<SchedulerValueType>& scheduler,
                            boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

/*!
 * Computes the sets of states that have probability greater 0 of satisfying phi until psi under at least
 * one possible resolution of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have a probability greater 0 of satisfying phi until psi if the
 * scheduler tries to minimize this probability.
 *
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @param useStepBound A flag that indicates whether or not to use the given number of maximal steps for the search.
 * @param maximalSteps The maximal number of steps to reach the psi states.
 * @return A bit vector that represents all states with probability 0.
 */
template<typename T>
storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template<typename T>
storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 1 of satisfying phi until psi under at least
 * one possible resolution of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have probability 1 of satisfying phi until psi if the
 * scheduler tries to maximize this probability.
 *
 * @param model The model whose graph structure to search.
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @param choiceConstraint If given, only the selected choices are considered.
 * @return A bit vector that represents all states with probability 1.
 */
template<typename T>
storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates,
                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

/*!
 * Computes the sets of states that have probability 1 of satisfying phi until psi under at least
 * one possible resolution of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have probability 1 of satisfying phi until psi if the
 * scheduler tries to maximize this probability.
 *
 * @param model The model whose graph structure to search.
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A bit vector that represents all states with probability 1.
 */
template<typename T, typename RM>
storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                 storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi
 * until psi in a non-deterministic model in which all non-deterministic choices are resolved
 * such that the probability is maximized.
 *
 * @param model The model whose graph structure to search.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A pair of bit vectors that represent all states with probability 0 and 1, respectively.
 */
template<typename T, typename RM>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability greater 0 of satisfying phi until psi under any
 * possible resolution of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have a probability greater 0 of satisfying phi until psi if the
 * scheduler tries to maximize this probability.
 *
 * @param model The model whose graph structure to search.
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @param useStepBound A flag that indicates whether or not to use the given number of maximal steps for the search.
 * @param maximalSteps The maximal number of steps to reach the psi states.
 * @param choiceConstraint If set, we assume that only the specified choices exist in the model
 * @return A bit vector that represents all states with probability 0.
 */
template<typename T>
storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                               std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                               storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                               boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

/*!
 * Computes the sets of states that have probability 0 of satisfying phi until psi under at least
 * one possible resolution of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have probability 0 of satisfying phi until psi if the
 * scheduler tries to minimize this probability.
 *
 * @param model The model whose graph structure to search.
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A bit vector that represents all states with probability 0.
 */
template<typename T, typename RM>
storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);
template<typename T>
storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 1 of satisfying phi until psi under all
 * possible resolutions of non-determinism in a non-deterministic model. Stated differently,
 * this means that these states have probability 1 of satisfying phi until psi even if the
 * scheduler tries to minimize this probability.
 *
 * @param model The model whose graph structure to search.
 * @param backwardTransitions The reversed transition relation of the model.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A bit vector that represents all states with probability 0.
 */
template<typename T, typename RM>
storm::storage::BitVector performProb1A(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);

template<typename T>
storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates);

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                 storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates);

/*!
 * Computes the sets of states that have probability 0 or 1, respectively, of satisfying phi
 * until psi in a non-deterministic model in which all non-deterministic choices are resolved
 * such that the probability is minimized.
 *
 * @param model The model whose graph structure to search.
 * @param phiStates The set of all states satisfying phi.
 * @param psiStates The set of all states satisfying psi.
 * @return A pair of bit vectors that represent all states with probability 0 and 1, respectively.
 */
template<typename T, typename RM>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates);

/*!
 * Computes the set of states for which there exists a scheduler that achieves a probability greater than
 * zero of satisfying phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProbGreater0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                          storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                          storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes a scheduler realizing a probability greater zero of satisfying phi until psi for all such states.
 */
template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> computeSchedulerProbGreater0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                   storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                   storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the set of states for which there does not exist a scheduler that achieves a probability greater
 * than zero of satisfying phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The phi states of the model.
 * @param psiStates The psi states of the model.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProb0A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the set of states for which all schedulers achieve a probability greater than zero of satisfying
 * phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProbGreater0A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                          storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                          storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the set of states for which there exists a scheduler that achieves probability zero of satisfying
 * phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProb0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

/*!
 * Computes the set of states for which all schedulers achieve probability one of satisfying phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param statesWithProbabilityGreater0A The states of the model that have a probability greater zero under
 * all schedulers.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProb1A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& psiStates, storm::dd::Bdd<Type> const& statesWithProbabilityGreater0A);

/*!
 * Computes the set of states for which there exists a scheduler that achieves probability one of satisfying
 * phi until psi.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param statesWithProbabilityGreater0E The states of the model that have a scheduler that achieves a value
 * greater than zero.
 * @return A BDD representing all such states.
 */
template<storm::dd::DdType Type, typename ValueType = double>
storm::dd::Bdd<Type> performProb1E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates,
                                   storm::dd::Bdd<Type> const& statesWithProbabilityGreater0E);

/*!
 * Computes a scheduler satisfying phi until psi with probability 1.
 */
template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> computeSchedulerProb1E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::dd::Bdd<Type> const& statesWithProbability1E);

template<storm::dd::DdType Type, typename ValueType = double>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Max(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

template<storm::dd::DdType Type, typename ValueType = double>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Max(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                       storm::dd::Bdd<Type> const& psiStates);

template<storm::dd::DdType Type, typename ValueType = double>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Min(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates);

template<storm::dd::DdType Type, typename ValueType = double>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Min(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                       storm::dd::Bdd<Type> const& psiStates);

template<storm::dd::DdType Type>
struct SymbolicGameProb01Result {
    SymbolicGameProb01Result() = default;
    SymbolicGameProb01Result(storm::dd::Bdd<Type> const& player1States, storm::dd::Bdd<Type> const& player2States,
                             boost::optional<storm::dd::Bdd<Type>> const& player1Strategy = boost::none,
                             boost::optional<storm::dd::Bdd<Type>> const& player2Strategy = boost::none)
        : player1States(player1States), player2States(player2States), player1Strategy(player1Strategy), player2Strategy(player2Strategy) {
        // Intentionally left empty.
    }

    bool hasPlayer1Strategy() const {
        return static_cast<bool>(player1Strategy);
    }

    storm::dd::Bdd<Type> const& getPlayer1Strategy() const {
        return player1Strategy.get();
    }

    boost::optional<storm::dd::Bdd<Type>> const& getOptionalPlayer1Strategy() {
        return player1Strategy;
    }

    bool hasPlayer2Strategy() const {
        return static_cast<bool>(player2Strategy);
    }

    storm::dd::Bdd<Type> const& getPlayer2Strategy() const {
        return player2Strategy.get();
    }

    boost::optional<storm::dd::Bdd<Type>> const& getOptionalPlayer2Strategy() {
        return player2Strategy;
    }

    storm::dd::Bdd<Type> const& getPlayer1States() const {
        return player1States;
    }

    storm::dd::Bdd<Type> const& getPlayer2States() const {
        return player2States;
    }

    storm::dd::Bdd<Type> player1States;
    storm::dd::Bdd<Type> player2States;
    boost::optional<storm::dd::Bdd<Type>> player1Strategy;
    boost::optional<storm::dd::Bdd<Type>> player2Strategy;
};

/*!
 * Computes the set of states that have probability 0 given the strategies of the two players.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param producePlayer1Strategy A flag indicating whether the strategy of player 1 shall be produced.
 * @param producePlayer2Strategy A flag indicating whether the strategy of player 2 shall be produced.
 */
template<storm::dd::DdType Type, typename ValueType>
SymbolicGameProb01Result<Type> performProb0(storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::OptimizationDirection const& player1Strategy,
                                            storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy = false,
                                            bool producePlayer2Strategy = false);

/*!
 * Computes the set of states that have probability 1 given the strategies of the two players.
 *
 * @param model The (symbolic) model for which to compute the set of states.
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param phiStates The BDD containing all phi states of the model.
 * @param psiStates The BDD containing all psi states of the model.
 * @param producePlayer1Strategy A flag indicating whether the strategy of player 1 shall be produced.
 * @param producePlayer2Strategy A flag indicating whether the strategy of player 2 shall be produced.
 * @param player1Candidates If given, this set constrains the candidates of player 1 states that are considered.
 */
template<storm::dd::DdType Type, typename ValueType>
SymbolicGameProb01Result<Type> performProb1(storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::OptimizationDirection const& player1Strategy,
                                            storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy = false,
                                            bool producePlayer2Strategy = false, boost::optional<storm::dd::Bdd<Type>> const& player1Candidates = boost::none);

struct ExplicitGameProb01Result {
    ExplicitGameProb01Result() = default;
    ExplicitGameProb01Result(uint64_t numberOfPlayer1States, uint64_t numberOfPlayer2States)
        : player1States(numberOfPlayer1States), player2States(numberOfPlayer2States) {
        // Intentionally left empty.
    }

    ExplicitGameProb01Result(storm::storage::BitVector const& player1States, storm::storage::BitVector const& player2States)
        : player1States(player1States), player2States(player2States) {
        // Intentionally left empty.
    }

    storm::storage::BitVector const& getPlayer1States() const {
        return player1States;
    }

    storm::storage::BitVector const& getPlayer2States() const {
        return player2States;
    }

    storm::storage::BitVector player1States;
    storm::storage::BitVector player2States;
};

/*!
 * Computes the set of states that have probability 0 given the strategies of the two players.
 *
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param player1Groups The grouping of player 1 states (in terms of player 2 states).
 * @param player1BackwardTransitions The backward transitions (player 1 to player 2).
 * @param player2BackwardTransitions The backward transitions (player 2 to player 1).
 * @param phiStates The phi states of the model.
 * @param psiStates The psi states of the model.
 * @param player1Direction The optimization direction of player 1.
 * @param player2Direction The optimization direction of player 2.
 * @param strategyPair If not null, player 1 and t2 strategies are synthesized and the corresponding choices
 * are written to this strategy.
 */
template<typename ValueType>
ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
                                      storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions,
                                      std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                      storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                      storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair = nullptr);

/*!
 * Computes the set of states that have probability 1 given the strategies of the two players.
 *
 * @param transitionMatrix The transition matrix of the model as a BDD.
 * @param player1Groups The grouping of player 1 states (in terms of player 2 states).
 * @param player1BackwardTransitions The backward transitions (player 1 to player 2).
 * @param player2BackwardTransitions The backward transitions (player 2 to player 1).
 * @param phiStates The phi states of the model.
 * @param psiStates The psi states of the model.
 * @param player1Direction The optimization direction of player 1.
 * @param player2Direction The optimization direction of player 2.
 * @param strategyPair If not null, player 1 and t2 strategies are synthesized and the corresponding choices
 * are written to this strategy.
 * @param player1Candidates If given, this set constrains the candidates of player 1 states that are considered.
 */
template<typename ValueType>
ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
                                      storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions,
                                      std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                      storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                      storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair = nullptr,
                                      boost::optional<storm::storage::BitVector> const& player1Candidates = boost::none);

/*!
 * Performs a topological sort of the states of the system according to the given transitions.
 *
 * @param matrix A square matrix representing the transition relation of the system.
 * @return A vector of indices that is a topological sort of the states.
 */
template<typename T>
std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<T> const& matrix, std::vector<uint64_t> const& firstStates = {});

template<typename T>
std::vector<uint_fast64_t> getBFSSort(storm::storage::SparseMatrix<T> const& matrix, std::vector<uint_fast64_t> const& firstStates);

}  // namespace graph
}  // namespace utility
}  // namespace storm
