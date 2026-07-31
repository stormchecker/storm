#include "storm/modelchecker/lexicographic/LexicographicModelCheckerHelper.h"

#include "storm//modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/automata/APSet.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/ExtractMaximalStateFormulasVisitor.h"
#include "storm/logic/Formula.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/lexicographic/spotHelper/SpotProduct.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/transformer/SubsystemBuilder.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace lexicographic {

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
std::pair<std::shared_ptr<storm::transformer::DAProduct<SparseModelType>>, std::vector<uint64_t>>
LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::getCompleteProductModel(const SparseModelType& model,
                                                                                                       CheckFormulaCallback const& formulaChecker) {
    storm::logic::ExtractMaximalStateFormulasVisitor::ApToFormulaMap extracted;
    std::vector<uint64_t> acceptanceConditions;

    // Get the big product automaton for all subformulae
    std::shared_ptr<storm::automata::DeterministicAutomaton> productAutomaton = spothelper::ltl2daSpotProduct(this->formula, extracted, acceptanceConditions);

    // Compute Satisfaction sets for the Atomic propositions (which represent the state-subformulae)
    std::map<std::string, storm::storage::BitVector> apSatSets = computeApSets(extracted, formulaChecker);
    const storm::automata::APSet& apSet = productAutomaton->getAPSet();
    std::vector<storm::storage::BitVector> statesForAP;
    for (const std::string& ap : apSet.getAPs()) {
        auto it = apSatSets.find(ap);
        STORM_LOG_THROW(it != apSatSets.end(), storm::exceptions::InvalidOperationException,
                        "Deterministic automaton has AP " << ap << ", does not appear in formula.");

        statesForAP.push_back(std::move(it->second));
    }

    storm::storage::BitVector statesOfInterest;

    if (this->hasRelevantStates()) {
        statesOfInterest = this->getRelevantStates();
    } else {
        // Product from all model states
        statesOfInterest = storm::storage::BitVector(this->_transitionMatrix.getRowGroupCount(), true);
    }

    // create the product of automaton and MDP
    transformer::DAProductBuilder productBuilder(*productAutomaton, statesForAP);
    std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> product =
        productBuilder.build<SparseModelType>(model.getTransitionMatrix(), statesOfInterest);

    return std::make_pair(product, acceptanceConditions);
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
std::pair<storm::storage::MaximalEndComponentDecomposition<ValueType>, std::vector<std::vector<bool>>>
LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::getLexArrays(
    std::shared_ptr<storm::transformer::DAProduct<productModelType>> productModel, std::vector<uint64_t>& acceptanceConditions) {
    storm::storage::BitVector allowed(productModel->getProductModel().getTransitionMatrix().getRowGroupCount(), true);
    // get MEC decomposition
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(productModel->getProductModel().getTransitionMatrix(),
                                                                     productModel->getProductModel().getBackwardTransitions(), allowed);

    std::vector<std::vector<bool>> bscc_satisfaction;
    storm::automata::AcceptanceCondition::ptr acceptance = productModel->getAcceptance();

    // Get all the Streett-pairs
    std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> acceptancePairs = getStreettPairs(acceptance->getAcceptanceExpression());
    STORM_LOG_ASSERT(!acceptancePairs.empty(), "There are no accepting pairs, maybe you have a parity automaton?");
    // they are ordered from last to first, so reverse the array
    std::reverse(acceptancePairs.begin(), acceptancePairs.end());

    // Iterate over the end-components and find their lex-array
    for (storm::storage::MaximalEndComponent& mec : mecs) {
        std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> sprime;
        std::vector<bool> bsccAccepting;
        for (uint64_t i = 0; i < acceptanceConditions.size() - 1; i++) {
            // copy the current list of Streett-pairs that can be fulfilled together
            std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> sprimeTemp(sprime);
            // add the new pairs (for the new condition) that should be checked now
            std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> sub = {acceptancePairs.begin() + acceptanceConditions[i],
                                                                                           acceptancePairs.begin() + acceptanceConditions[i + 1]};
            sprimeTemp.insert(sprimeTemp.end(), sub.begin(), sub.end());

            // check whether the Streett-condition in sprimeTemp can be fulfilled in the mec
            bool accepts = isAcceptingStreettConditions(mec, sprimeTemp, acceptance, productModel->getProductModel());

            if (accepts) {
                // if the condition can be fulfilled, add the Streett-pairs to the current list of pairs, and mark this property as true for this MEC
                bsccAccepting.push_back(true);
                sprime.insert(sprime.end(), sub.begin(), sub.end());
            } else {
                bsccAccepting.push_back(false);
            }
        }
        bscc_satisfaction.push_back(bsccAccepting);
    }
    return std::pair<storm::storage::MaximalEndComponentDecomposition<ValueType>&, std::vector<std::vector<bool>>&>(mecs, bscc_satisfaction);
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
MDPSparseModelCheckingHelperReturnType<ValueType> LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::lexReachability(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs, std::vector<std::vector<bool>> const& mecLexArray,
    std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> const& productModel, SparseModelType const& originalMdp) {
    // Eliminate all MECs and generate one sink state instead
    // Add first new states for each MEC
    auto stStateResult = addSinkStates(mecs, productModel);
    storm::storage::SparseMatrix<ValueType> newMatrixWithNewStates = stStateResult.first;
    std::map<uint64_t, uint64_t> bccToStStateMapping = stStateResult.second;

    // eliminate the MECs (collapse them into one state)
    storm::transformer::EndComponentEliminator<ValueType> eliminator = storm::transformer::EndComponentEliminator<ValueType>();
    storm::storage::BitVector eliminationStates(newMatrixWithNewStates.getColumnCount(), true);
    auto compressionResult =
        eliminator.transform(newMatrixWithNewStates, mecs, eliminationStates, storm::storage::BitVector(eliminationStates.size(), false), true);

    STORM_LOG_ASSERT(!mecLexArray.empty(), "No MECs in the model!");
    std::vector<std::vector<bool>> bccLexArrayCurrent(mecLexArray);
    // prepare the result (one reachability probability for each objective)
    MDPSparseModelCheckingHelperReturnType<ValueType> retResult(std::vector<ValueType>(mecLexArray[0].size()));
    std::vector<uint64_t> compressedToReducedMapping(compressionResult.matrix.getColumnCount());
    std::iota(std::begin(compressedToReducedMapping), std::end(compressedToReducedMapping), 0);
    storm::storage::SparseMatrix<ValueType> transitionMatrix = compressionResult.matrix;

    // check reachability for each condition and restrict the model to optimal choices
    for (uint64_t condition = 0; condition < mecLexArray[0].size(); condition++) {
        // get the goal-states for this objective (i.e. the st-states of the MECs where the objective can be fulfilled
        storm::storage::BitVector psiStates = getGoodStates(mecs, bccLexArrayCurrent, compressionResult.oldToNewStateMapping, condition,
                                                            transitionMatrix.getColumnCount(), compressedToReducedMapping, bccToStStateMapping);
        if (psiStates.getNumberOfSetBits() == 0) {
            retResult.values[condition] = 0;
            continue;
        }

        // solve the reachability query for this set of goal states
        std::vector<uint64_t> newInitalStates;
        auto res =
            solveOneReachability(newInitalStates, psiStates, transitionMatrix, originalMdp, compressedToReducedMapping, compressionResult.oldToNewStateMapping);
        if (newInitalStates.empty()) {
            retResult.values[condition] = 0;
            continue;
        }
        retResult.values[condition] = res.values[newInitalStates[0]];

        // create a reduced subsystem that only contains the optimal actions for this objective
        auto subsystem = getReducedSubsystem(transitionMatrix, res, newInitalStates, psiStates);
        std::vector<uint64_t> compressedToReducedMappingTemp;
        compressedToReducedMappingTemp.reserve(compressedToReducedMapping.size());
        // create a new mapping for the states
        for (uint64_t newState : subsystem.newToOldStateIndexMapping) {
            compressedToReducedMappingTemp.push_back(compressedToReducedMapping[newState]);
        }
        compressedToReducedMapping = compressedToReducedMappingTemp;
        transitionMatrix = subsystem.model->getTransitionMatrix();
    }
    return retResult;
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr>
LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::getStreettPairs(
    storm::automata::AcceptanceCondition::acceptance_expr::ptr const& current) {
    std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> accConds;
    if (current->isOR()) {
        accConds.push_back(current);
    } else if (current->isAND()) {
        auto leftAccConds = getStreettPairs(current->getLeft());
        accConds.reserve(accConds.size() + distance(leftAccConds.begin(), leftAccConds.end()));
        accConds.insert(accConds.end(), leftAccConds.begin(), leftAccConds.end());
        auto rightAccConds = getStreettPairs(current->getRight());
        accConds.reserve(accConds.size() + distance(rightAccConds.begin(), rightAccConds.end()));
        accConds.insert(accConds.end(), rightAccConds.begin(), rightAccConds.end());
    } else {
        STORM_LOG_THROW(true, storm::exceptions::NotImplementedException, "Finding StreettPairs - unknown type " + current->toString() + ".");
    }
    return accConds;
}

const storm::storage::BitVector& getStreettSet(storm::automata::AcceptanceCondition::ptr const& acceptance,
                                               storm::automata::AcceptanceCondition::acceptance_expr::ptr const& setPointer) {
    STORM_LOG_THROW(setPointer->isAtom(), storm::exceptions::NotImplementedException, "Not an Atom!");
    const cpphoafparser::AtomAcceptance& atom = setPointer->getAtom();
    const storm::storage::BitVector& accSet = acceptance->getAcceptanceSet(atom.getAcceptanceSet());
    return accSet;
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
bool LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::isAcceptingStreettConditions(
    storm::storage::MaximalEndComponent const& scc, std::vector<storm::automata::AcceptanceCondition::acceptance_expr::ptr> const& acceptancePairs,
    storm::automata::AcceptanceCondition::ptr const& acceptance, productModelType const& model) {
    // initialize the states and choices we have to consider for mec decomposition
    storm::storage::BitVector mecStates = storm::storage::BitVector(model.getNumberOfStates(), false);
    std::for_each(scc.begin(), scc.end(), [&mecStates](auto const& state) { mecStates.set(state.first); });
    auto mecChoices = model.getTransitionMatrix().getRowFilter(mecStates, mecStates);
    // catch the simple case where there are no mecs
    if (mecChoices.empty()) {
        return false;
    }
    // get easy access to incoming transitions of a state
    auto incomingChoicesMatrix = model.getTransitionMatrix().transpose();
    auto incomingStatesMatrix = model.getBackwardTransitions();
    bool changedSomething = true;
    while (changedSomething) {
        // iterate until there is no change
        changedSomething = false;
        // decompose the MEC, if possible
        auto subMecDecomposition =
            storm::storage::MaximalEndComponentDecomposition<ValueType>(model.getTransitionMatrix(), incomingStatesMatrix, mecStates, mecChoices);
        // iterate over all sub-MECs in the big MEC
        for (storm::storage::MaximalEndComponent const& mec : subMecDecomposition) {
            // iterate over all Streett-pairs
            for (storm::automata::AcceptanceCondition::acceptance_expr::ptr const& streettPair : acceptancePairs) {
                // check whether (i) the MEC contains states from the Inf-set (the condition holds) or (ii) states from the Fin-set (unclear whether it can be
                // fulfilled)
                auto infSet = getStreettSet(acceptance, streettPair->getRight());
                auto finSet = getStreettSet(acceptance, streettPair->getLeft());
                if (mec.containsAnyState(infSet)) {
                    // streett-condition is true (INF is fulfilled)
                    continue;
                } else {
                    // INF cannot be fulfilled
                    // check if there's a state from FIN
                    for (auto const& stateToChoice : mec) {
                        StateType state = stateToChoice.first;
                        if (finSet.get(state)) {
                            // remove the state from the set of states in this EC
                            mecStates.set(state, false);
                            // remove all incoming transitions to this state
                            auto incChoices = incomingChoicesMatrix.getRow(state);
                            std::for_each(incChoices.begin(), incChoices.end(), [&mecChoices](auto const& entry) { mecChoices.set(entry.getColumn(), false); });
                            changedSomething = true;
                        }
                    }
                }
            }
        }
    }
    // decompose one last time
    auto subMecDecomposition =
        storm::storage::MaximalEndComponentDecomposition<ValueType>(model.getTransitionMatrix(), incomingStatesMatrix, mecStates, mecChoices);
    if (subMecDecomposition.empty()) {
        // there are no more ECs in this set of states
        return false;
    } else {
        // there is still an end-component that can fulfill the Streett-condition
        return true;
    }
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
storm::storage::BitVector LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::getGoodStates(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& bcc, std::vector<std::vector<bool>> const& bccLexArray,
    std::vector<uint64_t> const& oldToNewStateMapping, uint64_t const& condition, uint64_t const numStates,
    std::vector<uint64_t> const& compressedToReducedMapping, std::map<uint64_t, uint64_t> const& bccToStStateMapping) {
    STORM_LOG_ASSERT(!bccLexArray.empty(), "Lex-Array is empty!");
    STORM_LOG_ASSERT(condition < bccLexArray[0].size(), "Condition is not in Lex-Array!");
    std::vector<uint64_t> goodStates;
    for (uint64_t i = 0; i < bcc.size(); i++) {
        std::vector<bool> const& bccLex = bccLexArray[i];
        if (bccLex[condition]) {
            uint64_t bccStateOld = bccToStStateMapping.at(i);
            uint64_t bccState = oldToNewStateMapping[bccStateOld];
            auto pointer = std::find(compressedToReducedMapping.begin(), compressedToReducedMapping.end(), bccState);
            if (pointer != compressedToReducedMapping.end()) {
                // We have to check whether the state has already been removed
                uint64_t bccStateReduced = pointer - compressedToReducedMapping.begin();
                goodStates.push_back(bccStateReduced);
            }
        }
    }
    return {numStates, goodStates};
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
MDPSparseModelCheckingHelperReturnType<ValueType> LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::solveOneReachability(
    std::vector<uint64_t>& newInitalStates, storm::storage::BitVector const& psiStates, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    SparseModelType const& originalMdp, std::vector<uint64_t> const& compressedToReducedMapping, std::vector<uint64_t> const& oldToNewStateMapping) {
    Environment env;
    // A reachability condition "F x" is transformed to "true U x"
    // phi states are all states
    // psi states are the ones from the "good bccs"
    storm::storage::BitVector phiStates(transitionMatrix.getColumnCount(), true);

    // Get initial states in the compressed model
    storm::storage::BitVector const& originalInitialStates = originalMdp.getInitialStates();

    uint64_t pos = 0;
    while (originalInitialStates.getNextSetIndex(pos) != originalInitialStates.size()) {
        pos = originalInitialStates.getNextSetIndex(pos) + 1;
        auto pointer = std::find(compressedToReducedMapping.begin(), compressedToReducedMapping.end(), oldToNewStateMapping[pos - 1]);
        if (pointer != compressedToReducedMapping.end()) {
            newInitalStates.push_back(pointer - compressedToReducedMapping.begin());
        }
    }
    storm::storage::BitVector i(transitionMatrix.getColumnCount(), newInitalStates);

    ModelCheckerHint hint;
    MDPSparseModelCheckingHelperReturnType<ValueType> ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType>::computeUntilProbabilities(
        env, storm::solver::SolveGoal<ValueType>(storm::solver::OptimizationDirection::Maximize, i), transitionMatrix, transitionMatrix.transpose(true),
        phiStates, psiStates, false, true, hint);
    return ret;
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
typename LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::SubsystemReturnType
LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::getReducedSubsystem(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, MDPSparseModelCheckingHelperReturnType<ValueType> const& reachabilityResult,
    std::vector<uint64_t> const& newInitalStates, storm::storage::BitVector const& goodStates) {
    std::vector<std::vector<uint64_t>> optimalActions;
    std::vector<uint64_t> keptActions;
    std::vector<uint64_t> const& rowGroupIndices = transitionMatrix.getRowGroupIndices();

    // iterate over the states
    for (uint64_t currentState = 0; currentState < reachabilityResult.values.size(); currentState++) {
        std::vector<uint64_t> goodActionsForState;
        uint64_t bestAction = reachabilityResult.scheduler->getChoice(currentState).getDeterministicChoice();
        // determine the value of the best action
        ValueType bestActionValue(0);
        for (const storm::storage::MatrixEntry<uint64_t, ValueType>& rowEntry : transitionMatrix.getRow(rowGroupIndices[currentState] + bestAction)) {
            bestActionValue += rowEntry.getValue() * reachabilityResult.values[rowEntry.getColumn()];
        }
        uint64_t lastAction = rowGroupIndices[currentState] + transitionMatrix.getRowGroupSize(currentState);
        // iterate over all actions in this state to find those that are also optimal
        for (uint64_t action = rowGroupIndices[currentState]; action < lastAction; action++) {
            ValueType actionValue(0);
            for (const auto& rowEntry : transitionMatrix.getRow(action)) {
                actionValue += rowEntry.getValue() * reachabilityResult.values[rowEntry.getColumn()];
            }
            if (actionValue == bestActionValue) {
                goodActionsForState.push_back(action);
                keptActions.push_back(action);
            }
        }
        optimalActions.push_back(goodActionsForState);
    }
    storm::storage::BitVector subSystemActions(transitionMatrix.getRowCount(), keptActions);
    storm::storage::BitVector subSystemStates(transitionMatrix.getRowGroupCount(), true);
    transformer::SubsystemBuilderOptions sbo;
    sbo.fixDeadlocks = true;
    storm::models::sparse::StateLabeling stateLabelling(transitionMatrix.getColumnCount());
    stateLabelling.addLabel("init");
    for (auto const& state : newInitalStates) {
        stateLabelling.addLabelToState("init", state);
    }
    storm::models::sparse::Mdp<ValueType> newModel(transitionMatrix, stateLabelling);
    SubsystemReturnType subsystemReturn = transformer::buildSubsystem(newModel, subSystemStates, subSystemActions, false, sbo);
    return subsystemReturn;
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
std::pair<storm::storage::SparseMatrix<ValueType>, std::map<uint64_t, uint64_t>>
LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::addSinkStates(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs,
    std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> const& productModel) {
    storm::storage::SparseMatrix<ValueType> const matrix = productModel->getProductModel().getTransitionMatrix();
    uint64_t countNewRows = 0;
    std::map<uint64_t, uint64_t> stateToMEC;
    for (uint64_t i = 0; i < mecs.size(); i++) {
        auto bcc = mecs[i];
        countNewRows += bcc.size() + 1;
        for (auto const& stateAndChoices : bcc) {
            // create an assignment of states to MECS
            uint64_t bccState = stateAndChoices.first;
            stateToMEC[bccState] = i;
        }
    }
    // create a new transition matrix
    storm::storage::SparseMatrixBuilder<ValueType> builder(matrix.getRowCount() + countNewRows, matrix.getColumnCount() + mecs.size(),
                                                           matrix.getEntryCount() + countNewRows, false, true, matrix.getRowGroupCount());
    uint64_t numRowGroups = matrix.getColumnCount() + mecs.size();
    std::map<uint64_t, uint64_t> sTstatesForBCC;
    uint64_t newestRowGroup = matrix.getRowGroupCount();
    uint64_t newRow = 0;
    uint64_t oldRowCounting = 0;
    auto oldRowGroupIndices = matrix.getRowGroupIndices();
    // iterate over the row groups (aka states) of the old transition matrix
    for (uint64_t newRowGroup = 0; newRowGroup < matrix.getColumnCount(); ++newRowGroup) {
        // create a new row group (aka state) for this
        builder.newRowGroup(newRow);
        // iterate over the rows of this row group (aka transitions from this state)
        for (; oldRowCounting < oldRowGroupIndices[newRowGroup + 1]; oldRowCounting++) {
            for (auto const& entry : matrix.getRow(oldRowCounting)) {
                // add the actions to the new matrix
                builder.addNextValue(newRow, entry.getColumn(), entry.getValue());
            }
            newRow++;
        }
        // if the state belongs to an MEC, add a new transition to the sink state of this MEC
        if (stateToMEC.find(newRowGroup) != stateToMEC.end()) {
            if (sTstatesForBCC.find(stateToMEC[newRowGroup]) == sTstatesForBCC.end()) {
                sTstatesForBCC[stateToMEC[newRowGroup]] = newestRowGroup;
                newestRowGroup++;
            }
            builder.addNextValue(newRow, sTstatesForBCC[stateToMEC[newRowGroup]], storm::utility::one<ValueType>());
            newRow++;
        }
    }
    // add new row groups (aka states) for the new sink states to the transition matrix
    // only possible action of those is a self-loop
    for (uint64_t newRowGroup = matrix.getColumnCount(); newRowGroup < numRowGroups; newRowGroup++) {
        builder.newRowGroup(newRow);
        builder.addNextValue(newRow, newRowGroup, storm::utility::one<ValueType>());
        newRow++;
    }
    storm::storage::SparseMatrix<ValueType> newMatrix = builder.build();
    return {newMatrix, sTstatesForBCC};
}

template<typename SparseModelType, typename ValueType, bool Nondeterministic>
std::map<std::string, storm::storage::BitVector> LexicographicModelCheckerHelper<SparseModelType, ValueType, Nondeterministic>::computeApSets(
    std::map<std::string, std::shared_ptr<storm::logic::Formula const>> const& extracted, CheckFormulaCallback const& formulaChecker) {
    std::map<std::string, storm::storage::BitVector> apSets;
    for (auto& p : extracted) {
        STORM_LOG_DEBUG(" Computing satisfaction set for atomic proposition \"" << p.first << "\" <=> " << *p.second << "...");
        apSets[p.first] = formulaChecker(*p.second);
    }
    return apSets;
}

template class lexicographic::LexicographicModelCheckerHelper<storm::models::sparse::Mdp<double>, double, true>;
template class lexicographic::LexicographicModelCheckerHelper<storm::models::sparse::Mdp<double>, double, false>;
template class lexicographic::LexicographicModelCheckerHelper<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber, true>;
template class lexicographic::LexicographicModelCheckerHelper<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber, false>;
}  // namespace lexicographic
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
