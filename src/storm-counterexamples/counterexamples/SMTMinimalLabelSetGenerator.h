#pragma once

#include <algorithm>
#include <chrono>
#include <iterator>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

#include "storm-counterexamples/counterexamples/GuaranteedLabelSet.h"
#include "storm-counterexamples/counterexamples/HighLevelCounterexample.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/ModelType.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Model.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CounterexampleGeneratorSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/solver/Z3SmtSolver.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/Program.h"
#include "storm/storage/sparse/JaniChoiceOrigins.h"
#include "storm/storage/sparse/PrismChoiceOrigins.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {

class Environment;

namespace counterexamples {

/**
 * Helper to avoid case disticinot between prism and jani
 * Returns the number of edges/commands in a symbolic model description.
 */
inline size_t nrCommands(storm::storage::SymbolicModelDescription const& descr) {
    if (descr.isJaniModel()) {
        return descr.asJaniModel().getNumberOfEdges();
    } else {
        STORM_LOG_ASSERT(descr.isPrismProgram(), "Expected Prism program.");
        return descr.asPrismProgram().getNumberOfCommands();
    }
}

/*!
 * This class provides functionality to generate a minimal counterexample to a probabilistic reachability
 * property in terms of used labels.
 */
template<class T>
class SMTMinimalLabelSetGenerator {
#ifdef STORM_HAVE_Z3
   private:
    struct RelevancyInformation {
        // The set of relevant states in the model.
        storm::storage::BitVector relevantStates;

        // The set of relevant labels.
        storm::storage::FlatSet<uint_fast64_t> relevantLabels;

        // The set of relevant label sets.
        storm::storage::FlatSet<storm::storage::FlatSet<uint_fast64_t>> relevantLabelSets;

        // The set of labels that matter in terms of minimality.
        storm::storage::FlatSet<uint_fast64_t> minimalityLabels;

        // A set of labels that is definitely known to be taken in the final solution.
        storm::storage::FlatSet<uint_fast64_t> knownLabels;

        storm::storage::FlatSet<uint64_t> dontCareLabels;

        // A list of relevant choices for each relevant state.
        std::map<uint_fast64_t, std::list<uint_fast64_t>> relevantChoicesForRelevantStates;
    };

    struct VariableInformation {
        // The manager responsible for the constraints we are building.
        std::shared_ptr<storm::expressions::ExpressionManager> manager;

        // The variables associated with the relevant labels.
        std::vector<storm::expressions::Variable> labelVariables;

        // The variables associated with the labels that matter in terms of minimality.
        std::vector<storm::expressions::Variable> minimalityLabelVariables;

        // A mapping from relevant labels to their indices in the variable vector.
        std::map<uint_fast64_t, uint_fast64_t> labelToIndexMap;

        // A set of original auxiliary variables needed for the Fu-Malik procedure.
        std::vector<storm::expressions::Variable> originalAuxiliaryVariables;

        // A set of auxiliary variables that may be modified by the MaxSAT procedure.
        std::vector<storm::expressions::Variable> auxiliaryVariables;

        // A vector of variables that can be used to constrain the number of variables that are set to true.
        std::vector<storm::expressions::Variable> adderVariables;

        // A flag whether or not there are variables reserved for encoding reachability of a target state.
        bool hasReachabilityVariables;

        // Starting from here, all structures hold information only if hasReachabilityVariables is true;

        // A mapping from each pair of adjacent relevant states to their index in the corresponding variable vector.
        std::map<std::pair<uint_fast64_t, uint_fast64_t>, uint_fast64_t> statePairToIndexMap;

        // A vector of variables associated with each pair of relevant states (s, s') such that s' is
        // a successor of s.
        std::vector<storm::expressions::Variable> statePairVariables;

        // A mapping from relevant states to the index with the corresponding order variable in the state order variable vector.
        std::map<uint_fast64_t, uint_fast64_t> relevantStatesToOrderVariableIndexMap;

        // A vector of variables that holds all state order variables.
        std::vector<storm::expressions::Variable> stateOrderVariables;
    };

    /*!
     * Computes the set of relevant labels in the model. Relevant labels are choice labels such that there exists
     * a scheduler that satisfies phi until psi with a nonzero probability.
     *
     * @param model The model to search for relevant labels.
     * @param phiStates A bit vector representing all states that satisfy phi.
     * @param psiStates A bit vector representing all states that satisfy psi.
     * @param dontCareLabels A set of labels that are "don't care" labels wrt. minimality.
     * @return A structure containing the relevant labels as well as states.
     */
    static RelevancyInformation determineRelevantStatesAndLabels(storm::models::sparse::Model<T> const& model,
                                                                 std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                                 storm::storage::FlatSet<uint_fast64_t> const& dontCareLabels) {
        // Create result.
        RelevancyInformation relevancyInformation;

        // Compute all relevant states, i.e. states for which there exists a scheduler that has a non-zero
        // probabilitiy of satisfying phi until psi.
        storm::storage::SparseMatrix<T> backwardTransitions = model.getBackwardTransitions();
        relevancyInformation.relevantStates = storm::utility::graph::performProbGreater0E(backwardTransitions, phiStates, psiStates);
        relevancyInformation.relevantStates &= ~psiStates;

        STORM_LOG_DEBUG("Found " << relevancyInformation.relevantStates.getNumberOfSetBits() << " relevant states.");

        // Retrieve some references for convenient access.
        storm::storage::SparseMatrix<T> const& transitionMatrix = model.getTransitionMatrix();
        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

        // Now traverse all choices of all relevant states and check whether there is a successor target state.
        // If so, the associated labels become relevant. Also, if a choice of a relevant state has at least one
        // relevant successor, the choice becomes relevant.
        for (auto state : relevancyInformation.relevantStates) {
            relevancyInformation.relevantChoicesForRelevantStates.emplace(state, std::list<uint_fast64_t>());

            for (uint_fast64_t row = nondeterministicChoiceIndices[state]; row < nondeterministicChoiceIndices[state + 1]; ++row) {
                for (auto const& entry : transitionMatrix.getRow(row)) {
                    // If there is a relevant successor, we need to add the labels of the current choice.
                    if (relevancyInformation.relevantStates.get(entry.getColumn()) || psiStates.get(entry.getColumn())) {
                        relevancyInformation.relevantChoicesForRelevantStates[state].push_back(row);

                        for (auto const& label : labelSets[row]) {
                            relevancyInformation.relevantLabels.insert(label);
                        }

                        relevancyInformation.relevantLabelSets.insert(labelSets[row]);
                        break;
                    }
                }
            }
        }

        // Compute the set of labels that are known to be taken in any case.
        relevancyInformation.knownLabels = storm::counterexamples::getGuaranteedLabelSet(model, labelSets, psiStates, relevancyInformation.relevantLabels);
        if (!relevancyInformation.knownLabels.empty()) {
            storm::storage::FlatSet<uint_fast64_t> remainingLabels;
            std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(),
                                relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                                std::inserter(remainingLabels, remainingLabels.end()));
            relevancyInformation.relevantLabels = remainingLabels;
        }

        relevancyInformation.dontCareLabels = dontCareLabels;
        std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(), dontCareLabels.begin(),
                            dontCareLabels.end(), std::inserter(relevancyInformation.minimalityLabels, relevancyInformation.minimalityLabels.begin()));

        STORM_LOG_DEBUG("Found " << relevancyInformation.relevantLabels.size() << " relevant and " << relevancyInformation.knownLabels.size()
                                 << " known labels.");
        STORM_LOG_DEBUG("Found " << relevancyInformation.minimalityLabels.size() << " labels to minize over.");
        return relevancyInformation;
    }

    /*!
     * Creates all necessary variables.
     *
     * @param manager The manager in which to create the variables.
     * @param relevantCommands A set of relevant labels for which to create the expressions.
     * @return A mapping from relevant labels to their corresponding expressions.
     */
    static VariableInformation createVariables(std::shared_ptr<storm::expressions::ExpressionManager> const& manager,
                                               storm::models::sparse::Model<T> const& model, storm::storage::BitVector const& psiStates,
                                               RelevancyInformation const& relevancyInformation, bool createReachabilityVariables) {
        VariableInformation variableInformation;
        variableInformation.manager = manager;

        // Create stringstream to build expression names.
        std::stringstream variableName;

        // Create the variables for the relevant labels.
        for (auto label : relevancyInformation.relevantLabels) {
            variableInformation.labelToIndexMap[label] = variableInformation.labelVariables.size();

            // Clear contents of the stream to construct new expression name.
            variableName.clear();
            variableName.str("");
            variableName << "c" << label;

            variableInformation.labelVariables.push_back(manager->declareBooleanVariable(variableName.str()));

            // Record if the label is among the ones that matter for minimality.
            if (relevancyInformation.minimalityLabels.find(label) != relevancyInformation.minimalityLabels.end()) {
                variableInformation.minimalityLabelVariables.push_back(variableInformation.labelVariables.back());
            }

            // Clear contents of the stream to construct new expression name.
            variableName.clear();
            variableName.str("");
            variableName << "h" << label;

            variableInformation.originalAuxiliaryVariables.push_back(manager->declareBooleanVariable(variableName.str()));
        }

        // A mapping from each pair of adjacent relevant states to their index in the corresponding variable vector.
        std::map<std::pair<uint_fast64_t, uint_fast64_t>, uint_fast64_t> statePairToIndexMap;

        // A vector of variables associated with each pair of relevant states (s, s') such that s' is
        // a successor of s.
        std::vector<storm::expressions::Variable> statePairVariables;

        // Create variables needed for encoding reachability of a target state if requested.
        if (createReachabilityVariables) {
            variableInformation.hasReachabilityVariables = true;

            storm::storage::SparseMatrix<T> const& transitionMatrix = model.getTransitionMatrix();

            for (auto state : relevancyInformation.relevantStates) {
                variableInformation.relevantStatesToOrderVariableIndexMap[state] = variableInformation.stateOrderVariables.size();

                // Clear contents of the stream to construct new expression name.
                variableName.clear();
                variableName.str("");
                variableName << "o" << state;

                variableInformation.stateOrderVariables.push_back(manager->declareRationalVariable(variableName.str()));

                for (auto const& relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(state)) {
                    for (auto const& entry : transitionMatrix.getRow(relevantChoice)) {
                        // If the successor state is neither the state itself nor an irrelevant state, we need to add a variable for the transition.
                        if (state != entry.getColumn() && (relevancyInformation.relevantStates.get(entry.getColumn()) || psiStates.get(entry.getColumn()))) {
                            // Make sure that there is not already one variable for the state pair. This may happen because of several nondeterministic choices
                            // targeting the same state.
                            if (variableInformation.statePairToIndexMap.find(std::make_pair(state, entry.getColumn())) !=
                                variableInformation.statePairToIndexMap.end()) {
                                continue;
                            }

                            // At this point we know that the state-pair does not have an associated variable.
                            variableInformation.statePairToIndexMap[std::make_pair(state, entry.getColumn())] = variableInformation.statePairVariables.size();

                            // Clear contents of the stream to construct new expression name.
                            variableName.clear();
                            variableName.str("");
                            variableName << "t" << state << "_" << entry.getColumn();

                            variableInformation.statePairVariables.push_back(manager->declareBooleanVariable(variableName.str()));
                        }
                    }
                }
            }

            for (auto psiState : psiStates) {
                variableInformation.relevantStatesToOrderVariableIndexMap[psiState] = variableInformation.stateOrderVariables.size();

                // Clear contents of the stream to construct new expression name.
                variableName.clear();
                variableName.str("");
                variableName << "o" << psiState;

                variableInformation.stateOrderVariables.push_back(manager->declareRationalVariable(variableName.str()));
            }
        }

        return variableInformation;
    }

    static storm::expressions::Expression getOtherSynchronizationEnabledFormula(
        storm::storage::FlatSet<uint_fast64_t> const& labelSet,
        std::map<uint_fast64_t, std::set<storm::storage::FlatSet<uint_fast64_t>>> const& synchronizingLabels,
        boost::container::flat_map<storm::storage::FlatSet<uint_fast64_t>, storm::expressions::Expression> const& labelSetToFormula,
        VariableInformation const& variableInformation, RelevancyInformation const& relevancyInformation) {
        // Taking all commands of a combination does not necessarily mean that a following label set needs to be taken.
        // This is because it could be that the commands are taken to enable other synchronizations. Therefore, we need
        // to add an additional clause that says that the right-hand side of the implication is also true if all commands
        // of the current choice have enabled synchronization options.
        if (labelSet.size() > 1) {
            storm::expressions::Expression result = variableInformation.manager->boolean(true);
            for (auto label : labelSet) {
                storm::expressions::Expression alternativeExpressionForLabel = variableInformation.manager->boolean(false);
                std::set<storm::storage::FlatSet<uint_fast64_t>> const& synchsForCommand = synchronizingLabels.at(label);

                for (auto const& synchSet : synchsForCommand) {
                    storm::expressions::Expression alternativeExpression = variableInformation.manager->boolean(true);

                    // If the current synchSet is the same as left-hand side of the implication, we can to skip it.
                    if (synchSet == labelSet)
                        continue;

                    // Build labels that are missing for this synchronization option.
                    std::set<uint_fast64_t> unknownSynchSetLabels;
                    std::set_difference(synchSet.begin(), synchSet.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                                        std::inserter(unknownSynchSetLabels, unknownSynchSetLabels.end()));

                    for (auto label : unknownSynchSetLabels) {
                        alternativeExpression = alternativeExpression && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                    }

                    alternativeExpressionForLabel = alternativeExpressionForLabel || (alternativeExpression && labelSetToFormula.at(synchSet));
                }

                result = result && alternativeExpressionForLabel;
            }

            return result;
        } else {
            return variableInformation.manager->boolean(false);
        }
    }

    /*!
     * Asserts cuts that are derived from the explicit representation of the model and rule out a lot of
     * suboptimal solutions.
     *
     * @param symbolicModel The symbolic model description used to build the model.
     * @param model The labeled model for which to compute the cuts.
     * @param context The Z3 context in which to build the expressions.
     * @param solver The solver to use for the satisfiability evaluation.
     */
    static std::chrono::milliseconds assertCuts(storm::storage::SymbolicModelDescription const& symbolicModel, storm::models::sparse::Model<T> const& model,
                                                std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                storm::storage::BitVector const& psiStates, VariableInformation const& variableInformation,
                                                RelevancyInformation const& relevancyInformation, storm::solver::SmtSolver& solver,
                                                bool addBackwardImplications) {
        // Walk through the model and
        // * identify labels enabled in initial states
        // * identify labels that can directly precede a given action
        // * identify labels that directly reach a target state
        // * identify labels that can directly follow a given action
        auto assertCutsClock = std::chrono::high_resolution_clock::now();

        //
        if (addBackwardImplications) {
            STORM_LOG_THROW(!symbolicModel.isJaniModel() || !symbolicModel.asJaniModel().usesAssignmentLevels(), storm::exceptions::NotSupportedException,
                            "Counterexample generation with backward implications is not supported for indexed assignments.");
        }

        storm::storage::FlatSet<uint_fast64_t> initialLabels;
        std::set<storm::storage::FlatSet<uint_fast64_t>> initialCombinations;
        storm::storage::FlatSet<uint_fast64_t> targetLabels;
        storm::storage::FlatSet<storm::storage::FlatSet<uint_fast64_t>> targetCombinations;
        std::map<storm::storage::FlatSet<uint_fast64_t>, std::set<storm::storage::FlatSet<uint_fast64_t>>> precedingLabels;
        std::map<storm::storage::FlatSet<uint_fast64_t>, std::set<storm::storage::FlatSet<uint_fast64_t>>> followingLabels;
        std::map<uint_fast64_t, std::set<storm::storage::FlatSet<uint_fast64_t>>> synchronizingLabels;

        // Get some data from the model for convenient access.
        storm::storage::SparseMatrix<T> const& transitionMatrix = model.getTransitionMatrix();
        storm::storage::SparseMatrix<T> backwardTransitions = model.getBackwardTransitions();
        storm::storage::BitVector const& initialStates = model.getInitialStates();

        for (auto currentState : relevancyInformation.relevantStates) {
            bool isInitial = initialStates.get(currentState);
            for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(currentState)) {
                // If the choice is a synchronization choice, we need to record it.
                if (labelSets[currentChoice].size() > 1) {
                    for (auto label : labelSets[currentChoice]) {
                        synchronizingLabels[label].emplace(labelSets[currentChoice]);
                    }
                }

                // If the state is initial, we need to add all the choice labels to the initial label set.
                if (isInitial) {
                    initialLabels.insert(labelSets[currentChoice].begin(), labelSets[currentChoice].end());
                    initialCombinations.insert(labelSets[currentChoice]);
                }

                // Iterate over successors and add relevant choices of relevant successors to the following label set.
                bool canReachTargetState = false;
                for (auto const& entry : transitionMatrix.getRow(currentChoice)) {
                    if (relevancyInformation.relevantStates.get(entry.getColumn())) {
                        for (auto relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(entry.getColumn())) {
                            if (labelSets[currentChoice] == labelSets[relevantChoice])
                                continue;

                            followingLabels[labelSets[currentChoice]].insert(labelSets[relevantChoice]);
                        }
                    } else if (psiStates.get(entry.getColumn())) {
                        canReachTargetState = true;
                    }
                }

                // If the choice can reach a target state directly, we add all the labels to the target label set.
                if (canReachTargetState) {
                    targetLabels.insert(labelSets[currentChoice].begin(), labelSets[currentChoice].end());
                    targetCombinations.insert(labelSets[currentChoice]);
                }

                // Iterate over predecessors and add all choices that target the current state to the preceding
                // label set of all labels of all relevant choices of the current state.
                for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
                    if (relevancyInformation.relevantStates.get(predecessorEntry.getColumn())) {
                        for (auto predecessorChoice : relevancyInformation.relevantChoicesForRelevantStates.at(predecessorEntry.getColumn())) {
                            bool choiceTargetsCurrentState = false;
                            for (auto const& successorEntry : transitionMatrix.getRow(predecessorChoice)) {
                                if (successorEntry.getColumn() == currentState) {
                                    choiceTargetsCurrentState = true;
                                    break;
                                }
                            }

                            if (choiceTargetsCurrentState) {
                                precedingLabels[labelSets.at(currentChoice)].insert(labelSets.at(predecessorChoice));
                            }
                        }
                    }
                }
            }
        }

        // Store the found implications in a container similar to the preceding label sets.
        std::map<storm::storage::FlatSet<uint_fast64_t>, std::set<storm::storage::FlatSet<uint_fast64_t>>> backwardImplications;
        if (addBackwardImplications) {
            // Create a new solver over the same variables as the given symbolic model description to use it for
            // determining the symbolic cuts.
            std::unique_ptr<storm::solver::SmtSolver> localSolver;
            if (symbolicModel.isPrismProgram()) {
                storm::prism::Program const& program = symbolicModel.asPrismProgram();
                localSolver = std::make_unique<storm::solver::Z3SmtSolver>(program.getManager());

                // Then add the constraints for bounds of the integer variables.
                for (auto const& integerVariable : program.getGlobalIntegerVariables()) {
                    localSolver->add(integerVariable.getRangeExpression());
                }
                for (auto const& module : program.getModules()) {
                    for (auto const& integerVariable : module.getIntegerVariables()) {
                        localSolver->add(integerVariable.getRangeExpression());
                    }
                }
            } else {
                storm::jani::Model const& janiModel = symbolicModel.asJaniModel();
                localSolver = std::make_unique<storm::solver::Z3SmtSolver>(janiModel.getManager());

                for (auto const& integerVariable : janiModel.getGlobalVariables().getBoundedIntegerVariables()) {
                    if (integerVariable.isTransient()) {
                        continue;
                    }
                    localSolver->add(integerVariable.getRangeExpression());
                }
                for (auto const& automaton : janiModel.getAutomata()) {
                    for (auto const& integerVariable : automaton.getVariables().getBoundedIntegerVariables()) {
                        if (integerVariable.isTransient()) {
                            continue;
                        }
                        localSolver->add(integerVariable.getRangeExpression());
                    }
                }
            }

            // Construct an expression that exactly characterizes the initial state.
            storm::expressions::Expression initialStateExpression;
            if (symbolicModel.isPrismProgram()) {
                initialStateExpression = symbolicModel.asPrismProgram().getInitialStatesExpression();
            } else {
                initialStateExpression = symbolicModel.asJaniModel().getInitialStatesExpression();
            }

            // Now check for possible backward cuts.
            for (auto const& labelSetAndPrecedingLabelSetsPair : precedingLabels) {
                bool backwardImplicationAdded = false;
                //                        std::cout << "labelSetAndPrecedingLabelSetsPair.first ";
                //                        for (auto const& e : labelSetAndPrecedingLabelSetsPair.first) {
                //                            std::cout << e << ", ";
                //                        }
                //                        std::cout << '\n';
                // Find out the commands for the currently considered label set.
                storm::expressions::Expression guardConjunction;

                if (symbolicModel.isPrismProgram()) {
                    storm::prism::Program const& program = symbolicModel.asPrismProgram();

                    for (uint_fast64_t moduleIndex = 0; moduleIndex < program.getNumberOfModules(); ++moduleIndex) {
                        storm::prism::Module const& module = program.getModule(moduleIndex);

                        for (uint_fast64_t commandIndex = 0; commandIndex < module.getNumberOfCommands(); ++commandIndex) {
                            storm::prism::Command const& command = module.getCommand(commandIndex);

                            // If the current command is one of the commands we need to consider, add its guard.
                            if (labelSetAndPrecedingLabelSetsPair.first.find(command.getGlobalIndex()) != labelSetAndPrecedingLabelSetsPair.first.end()) {
                                guardConjunction = guardConjunction && command.getGuardExpression();
                            }
                        }
                    }
                } else {
                    storm::jani::Model const& janiModel = symbolicModel.asJaniModel();

                    for (uint_fast64_t automatonIndex = 0; automatonIndex < janiModel.getNumberOfAutomata(); ++automatonIndex) {
                        storm::jani::Automaton const& automaton = janiModel.getAutomaton(automatonIndex);

                        for (uint_fast64_t edgeIndex = 0; edgeIndex < automaton.getNumberOfEdges(); ++edgeIndex) {
                            // If the current edge is one of the edges we need to consider, add its guard.
                            if (labelSetAndPrecedingLabelSetsPair.first.find(janiModel.encodeAutomatonAndEdgeIndices(automatonIndex, edgeIndex)) !=
                                labelSetAndPrecedingLabelSetsPair.first.end()) {
                                storm::jani::Edge const& edge = automaton.getEdge(edgeIndex);

                                guardConjunction = guardConjunction && edge.getGuard();
                            }
                        }
                    }
                }

                // Save the state of the solver so we can easily backtrack.
                localSolver->push();

                // Push initial state expression.
                STORM_LOG_DEBUG("About to assert that combination is not enabled in the current state.");
                localSolver->add(initialStateExpression);

                // Check if the label set is enabled in the initial state.
                localSolver->add(guardConjunction);

                storm::solver::SmtSolver::CheckResult checkResult = localSolver->check();
                localSolver->pop();
                localSolver->push();

                // If the solver reports unsat, then we know that the current selection is not enabled in the initial state.
                if (checkResult == storm::solver::SmtSolver::CheckResult::Unsat) {
                    STORM_LOG_DEBUG("Selection not enabled in initial state.");

                    // std::cout << "not gc: " << !guardConjunction << '\n';
                    localSolver->add(!guardConjunction);
                    STORM_LOG_DEBUG("Asserted disjunction of negated guards.");

                    // Now check the possible preceding label sets for the essential ones.
                    for (auto const& precedingLabelSet : labelSetAndPrecedingLabelSetsPair.second) {
                        if (labelSetAndPrecedingLabelSetsPair.first == precedingLabelSet)
                            continue;

                        // std::cout << "push\n";
                        // Create a restore point so we can easily pop-off all weakest precondition expressions.
                        localSolver->push();

                        // Find out the commands for the currently considered preceding label set.
                        std::vector<std::vector<boost::container::flat_map<storm::expressions::Variable, storm::expressions::Expression>>>
                            currentPreceedingVariableUpdates;
                        storm::expressions::Expression preceedingGuardConjunction;
                        if (symbolicModel.isPrismProgram()) {
                            storm::prism::Program const& program = symbolicModel.asPrismProgram();
                            for (uint_fast64_t moduleIndex = 0; moduleIndex < program.getNumberOfModules(); ++moduleIndex) {
                                storm::prism::Module const& module = program.getModule(moduleIndex);

                                for (uint_fast64_t commandIndex = 0; commandIndex < module.getNumberOfCommands(); ++commandIndex) {
                                    storm::prism::Command const& command = module.getCommand(commandIndex);

                                    // If the current command is one of the commands we need to consider, store a reference to it in the container.
                                    if (precedingLabelSet.find(command.getGlobalIndex()) != precedingLabelSet.end()) {
                                        preceedingGuardConjunction = preceedingGuardConjunction && command.getGuardExpression();

                                        currentPreceedingVariableUpdates.emplace_back();

                                        for (uint64_t updateIndex = 0; updateIndex < command.getNumberOfUpdates(); ++updateIndex) {
                                            storm::prism::Update const& update = command.getUpdate(updateIndex);
                                            boost::container::flat_map<storm::expressions::Variable, storm::expressions::Expression> variableUpdates;
                                            for (auto const& assignment : update.getAssignments()) {
                                                variableUpdates.emplace(assignment.getVariable(), assignment.getExpression());
                                            }
                                            currentPreceedingVariableUpdates.back().emplace_back(std::move(variableUpdates));
                                        }
                                    }
                                }
                            }
                        } else {
                            storm::jani::Model const& janiModel = symbolicModel.asJaniModel();
                            for (uint_fast64_t automatonIndex = 0; automatonIndex < janiModel.getNumberOfAutomata(); ++automatonIndex) {
                                storm::jani::Automaton const& automaton = janiModel.getAutomaton(automatonIndex);

                                for (uint_fast64_t edgeIndex = 0; edgeIndex < automaton.getNumberOfEdges(); ++edgeIndex) {
                                    // If the current command is one of the commands we need to consider, store a reference to it in the container.
                                    if (precedingLabelSet.find(janiModel.encodeAutomatonAndEdgeIndices(automatonIndex, edgeIndex)) != precedingLabelSet.end()) {
                                        storm::jani::Edge const& edge = automaton.getEdge(edgeIndex);

                                        preceedingGuardConjunction = preceedingGuardConjunction && edge.getGuard();

                                        currentPreceedingVariableUpdates.emplace_back();

                                        for (uint64_t destinationIndex = 0; destinationIndex < edge.getNumberOfDestinations(); ++destinationIndex) {
                                            storm::jani::EdgeDestination const& destination = edge.getDestination(destinationIndex);
                                            boost::container::flat_map<storm::expressions::Variable, storm::expressions::Expression> variableUpdates;
                                            for (auto const& assignment : destination.getOrderedAssignments().getNonTransientAssignments()) {
                                                variableUpdates.emplace(assignment.getVariable().getExpressionVariable(), assignment.getAssignedExpression());
                                            }
                                            currentPreceedingVariableUpdates.back().emplace_back(std::move(variableUpdates));
                                        }
                                    }
                                }
                            }
                        }

                        // std::cout << "pgc: " << preceedingGuardConjunction << '\n';

                        // Assert all the guards of the preceding command set.
                        localSolver->add(preceedingGuardConjunction);

                        std::vector<std::vector<boost::container::flat_map<storm::expressions::Variable, storm::expressions::Expression>>::const_iterator>
                            iteratorVector;
                        for (auto const& variableUpdates : currentPreceedingVariableUpdates) {
                            iteratorVector.push_back(variableUpdates.begin());
                        }

                        // Iterate over all possible combinations of updates of the preceding command set.
                        std::vector<storm::expressions::Expression> formulae;
                        while (true) {
                            std::map<storm::expressions::Variable, storm::expressions::Expression> currentVariableUpdateCombinationMap;
                            for (auto const& updateIterator : iteratorVector) {
                                for (auto const& variableUpdatePair : *updateIterator) {
                                    currentVariableUpdateCombinationMap.emplace(variableUpdatePair.first, variableUpdatePair.second);
                                }
                            }

                            STORM_LOG_DEBUG("About to assert a weakest precondition.");
                            storm::expressions::Expression wp = guardConjunction.substitute(currentVariableUpdateCombinationMap);
                            formulae.push_back(wp);
                            STORM_LOG_DEBUG("Asserted weakest precondition.");

                            // Now try to move iterators to the next position if possible. If we could properly
                            // move it, we can directly move on to the next combination of updates. If we have
                            // to reset it to the start, we do so unless there is no more iterator to reset.
                            uint_fast64_t k = iteratorVector.size();
                            for (; k > 0; --k) {
                                ++iteratorVector[k - 1];
                                if (iteratorVector[k - 1] == currentPreceedingVariableUpdates[k - 1].end()) {
                                    iteratorVector[k - 1] = currentPreceedingVariableUpdates[k - 1].begin();
                                } else {
                                    break;
                                }
                            }

                            // If we had to reset all iterator to the start, we are done.
                            if (k == 0) {
                                break;
                            }
                        }

                        // Now assert the disjunction of all weakest preconditions of all considered update combinations.
                        assertDisjunction(
                            *localSolver, formulae,
                            symbolicModel.isPrismProgram() ? symbolicModel.asPrismProgram().getManager() : symbolicModel.asJaniModel().getManager());

                        STORM_LOG_DEBUG("Asserted disjunction of all weakest preconditions.");
                        storm::solver::SmtSolver::CheckResult result = localSolver->check();

                        if (result == storm::solver::SmtSolver::CheckResult::Sat) {
                            backwardImplications[labelSetAndPrecedingLabelSetsPair.first].insert(precedingLabelSet);
                            backwardImplicationAdded = true;
                        } else if (result == storm::solver::SmtSolver::CheckResult::Unknown) {
                            STORM_LOG_ERROR("The SMT solver does not come to a conclusive answer. Does your model contain integer division?");
                        }

                        localSolver->pop();
                    }

                    // Popping the disjunction of negated guards from the solver stack.
                    localSolver->pop();
                    STORM_LOG_ERROR_COND(backwardImplicationAdded,
                                         "Error in adding cuts for counterexample generation (backward implication misses a label set).");
                } else {
                    STORM_LOG_DEBUG("Selection is enabled in initial state.");
                }
            }
        } else if (symbolicModel.isJaniModel()) {
            STORM_LOG_WARN("Model uses assignment levels, did not assert backward implications.");
        }

        STORM_LOG_DEBUG("Successfully gathered data for cuts.");

        // Compute the sets of labels such that the transitions labeled with this set possess at least one known label.
        storm::storage::FlatSet<storm::storage::FlatSet<uint_fast64_t>> hasKnownSuccessor;
        for (auto const& labelSetFollowingSetsPair : followingLabels) {
            for (auto const& set : labelSetFollowingSetsPair.second) {
                if (std::includes(relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), set.begin(), set.end())) {
                    hasKnownSuccessor.insert(set);
                    break;
                }
            }
        }

        // Compute the sets of labels such that the transitions labeled with this set possess at least one known predecessor.
        storm::storage::FlatSet<storm::storage::FlatSet<uint_fast64_t>> hasKnownPredecessor;
        for (auto const& labelSetImplicationsPair : backwardImplications) {
            for (auto const& set : labelSetImplicationsPair.second) {
                if (std::includes(relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(), set.begin(), set.end())) {
                    hasKnownPredecessor.insert(labelSetImplicationsPair.first);
                    break;
                }
            }
        }

        STORM_LOG_DEBUG("Asserting initial combination is taken.");
        {
            std::vector<storm::expressions::Expression> formulae;

            // Start by asserting that we take at least one initial label. We may do so only if there is no initial
            // combination that is already known. Otherwise this condition would be too strong.
            bool initialCombinationKnown = false;
            for (auto const& combination : initialCombinations) {
                storm::storage::FlatSet<uint_fast64_t> tmpSet;
                std::set_difference(combination.begin(), combination.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                                    std::inserter(tmpSet, tmpSet.end()));
                if (tmpSet.size() == 0) {
                    initialCombinationKnown = true;
                    break;
                } else {
                    storm::expressions::Expression conj = variableInformation.manager->boolean(true);
                    for (auto label : tmpSet) {
                        conj = conj && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                    }
                    formulae.push_back(conj);
                }
            }
            if (!initialCombinationKnown) {
                assertDisjunction(solver, formulae, *variableInformation.manager);
            }
        }

        STORM_LOG_DEBUG("Asserting target combination is taken.");
        {
            std::vector<storm::expressions::Expression> formulae;

            // Likewise, if no target combination is known, we may assert that there is at least one.
            bool targetCombinationKnown = false;
            for (auto const& combination : targetCombinations) {
                storm::storage::FlatSet<uint_fast64_t> tmpSet;
                std::set_difference(combination.begin(), combination.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                                    std::inserter(tmpSet, tmpSet.end()));
                if (tmpSet.size() == 0) {
                    targetCombinationKnown = true;
                    break;
                } else {
                    storm::expressions::Expression conj = variableInformation.manager->boolean(true);
                    for (auto label : tmpSet) {
                        conj = conj && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                    }
                    formulae.push_back(conj);
                }
            }
            if (!targetCombinationKnown) {
                assertDisjunction(solver, formulae, *variableInformation.manager);
            }
        }

        if (addBackwardImplications) {
            STORM_LOG_DEBUG(
                "Asserting taken labels are followed and preceeded by another label if they are not a target label or an initial label, respectively.");
            boost::container::flat_map<storm::storage::FlatSet<uint_fast64_t>, storm::expressions::Expression> labelSetToFormula;
            for (auto const& labelSet : relevancyInformation.relevantLabelSets) {
                storm::expressions::Expression labelSetFormula = variableInformation.manager->boolean(false);

                // Compute the set of unknown labels on the left-hand side of the implication.
                storm::storage::FlatSet<uint_fast64_t> unknownLhsLabels;
                std::set_difference(labelSet.begin(), labelSet.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                                    std::inserter(unknownLhsLabels, unknownLhsLabels.end()));
                for (auto label : unknownLhsLabels) {
                    labelSetFormula = labelSetFormula || !variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                }

                // Only build a constraint if the combination does not lead to a target state and
                // no successor set is already known.
                storm::expressions::Expression successorExpression;
                if (targetCombinations.find(labelSet) == targetCombinations.end() && hasKnownSuccessor.find(labelSet) == hasKnownSuccessor.end()) {
                    successorExpression = variableInformation.manager->boolean(false);

                    auto const& followingLabelSets = followingLabels.at(labelSet);

                    for (auto const& followingSet : followingLabelSets) {
                        storm::storage::FlatSet<uint_fast64_t> tmpSet;

                        // Check which labels of the current following set are not known. This set must be non-empty, because
                        // otherwise a successor combination would already be known and control cannot reach this point.
                        std::set_difference(followingSet.begin(), followingSet.end(), relevancyInformation.knownLabels.begin(),
                                            relevancyInformation.knownLabels.end(), std::inserter(tmpSet, tmpSet.end()));

                        // Construct an expression that enables all unknown labels of the current following set.
                        storm::expressions::Expression conj = variableInformation.manager->boolean(true);
                        for (auto label : tmpSet) {
                            conj = conj && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                        }
                        successorExpression = successorExpression || conj;
                    }
                } else {
                    successorExpression = variableInformation.manager->boolean(true);
                }
                // Constructed following cuts at this point.

                // Only build a constraint if the combination is no initial combination and no
                // predecessor set is already known.
                storm::expressions::Expression predecessorExpression;
                if (initialCombinations.find(labelSet) == initialCombinations.end() && hasKnownPredecessor.find(labelSet) == hasKnownPredecessor.end()) {
                    predecessorExpression = variableInformation.manager->boolean(false);

                    //                        std::cout << "labelSet\n";
                    //                        for (auto const& e : labelSet) {
                    //                            std::cout << e << ", ";
                    //                        }
                    //                        std::cout << '\n';

                    auto const& preceedingLabelSets = backwardImplications.at(labelSet);

                    for (auto const& preceedingSet : preceedingLabelSets) {
                        storm::storage::FlatSet<uint_fast64_t> tmpSet;

                        // Check which labels of the current following set are not known. This set must be non-empty, because
                        // otherwise a predecessor combination would already be known and control cannot reach this point.
                        std::set_difference(preceedingSet.begin(), preceedingSet.end(), relevancyInformation.knownLabels.begin(),
                                            relevancyInformation.knownLabels.end(), std::inserter(tmpSet, tmpSet.end()));

                        // Construct an expression that enables all unknown labels of the current following set.
                        storm::expressions::Expression conj = variableInformation.manager->boolean(true);
                        for (auto label : tmpSet) {
                            conj = conj && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                        }
                        predecessorExpression = predecessorExpression || conj;
                    }
                } else {
                    predecessorExpression = variableInformation.manager->boolean(true);
                }

                labelSetFormula = labelSetFormula || (successorExpression && predecessorExpression);

                labelSetToFormula[labelSet] = labelSetFormula;
            }

            for (auto const& labelSetFormula : labelSetToFormula) {
                solver.add(labelSetFormula.second || getOtherSynchronizationEnabledFormula(labelSetFormula.first, synchronizingLabels, labelSetToFormula,
                                                                                           variableInformation, relevancyInformation));
            }
        }

        STORM_LOG_DEBUG("Asserting synchronization cuts.");
        // Finally, assert that if we take one of the synchronizing labels, we also take one of the combinations
        // the label appears in.
        for (auto const& labelSynchronizingSetsPair : synchronizingLabels) {
            std::vector<storm::expressions::Expression> formulae;

            if (relevancyInformation.knownLabels.find(labelSynchronizingSetsPair.first) == relevancyInformation.knownLabels.end()) {
                formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(labelSynchronizingSetsPair.first)));
            }

            // We need to be careful, because there may be one synchronisation set out of which all labels are
            // known, which means we must not assert anything.
            bool allImplicantsKnownForOneSet = false;
            for (auto const& synchronizingSet : labelSynchronizingSetsPair.second) {
                storm::expressions::Expression currentCombination = variableInformation.manager->boolean(true);
                bool allImplicantsKnownForCurrentSet = true;
                for (auto label : synchronizingSet) {
                    if (relevancyInformation.knownLabels.find(label) == relevancyInformation.knownLabels.end() && label != labelSynchronizingSetsPair.first) {
                        currentCombination = currentCombination && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label));
                    }
                }
                formulae.push_back(currentCombination);

                // If all implicants of the current set are known, we do not need to further build the constraint.
                if (allImplicantsKnownForCurrentSet) {
                    allImplicantsKnownForOneSet = true;
                    break;
                }
            }

            if (!allImplicantsKnownForOneSet) {
                assertDisjunction(solver, formulae, *variableInformation.manager);
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - assertCutsClock);
    }

    /*!
     * Asserts constraints necessary to encode the reachability of at least one target state from the initial states.
     */
    static void assertReachabilityCuts(storm::models::sparse::Model<T> const& model, std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                       storm::storage::BitVector const& psiStates, VariableInformation const& variableInformation,
                                       RelevancyInformation const& relevancyInformation, storm::solver::SmtSolver& solver) {
        STORM_LOG_THROW(variableInformation.hasReachabilityVariables, storm::exceptions::InvalidStateException,
                        "Impossible to assert reachability cuts without the necessary variables.");

        // Get some data from the model for convenient access.
        storm::storage::SparseMatrix<T> const& transitionMatrix = model.getTransitionMatrix();
        storm::storage::SparseMatrix<T> backwardTransitions = model.getBackwardTransitions();

        // First, we add the formulas that encode
        // (1) if an incoming transition is chosen, an outgoing one is chosen as well (for non-initial states)
        // (2) an outgoing transition out of the initial states is taken.
        storm::expressions::Expression initialStateExpression = variableInformation.manager->boolean(false);
        for (auto relevantState : relevancyInformation.relevantStates) {
            if (!model.getInitialStates().get(relevantState)) {
                // Assert the constraints (1).
                storm::storage::FlatSet<uint_fast64_t> relevantPredecessors;
                for (auto const& predecessorEntry : backwardTransitions.getRow(relevantState)) {
                    if (relevantState != predecessorEntry.getColumn() && relevancyInformation.relevantStates.get(predecessorEntry.getColumn())) {
                        relevantPredecessors.insert(predecessorEntry.getColumn());
                    }
                }

                storm::storage::FlatSet<uint_fast64_t> relevantSuccessors;
                for (auto const& relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(relevantState)) {
                    for (auto const& successorEntry : transitionMatrix.getRow(relevantChoice)) {
                        if (relevantState != successorEntry.getColumn() &&
                            (relevancyInformation.relevantStates.get(successorEntry.getColumn()) || psiStates.get(successorEntry.getColumn()))) {
                            relevantSuccessors.insert(successorEntry.getColumn());
                        }
                    }
                }

                storm::expressions::Expression expression = variableInformation.manager->boolean(true);
                for (auto predecessor : relevantPredecessors) {
                    expression = expression && !variableInformation.statePairVariables.at(
                                                   variableInformation.statePairToIndexMap.at(std::make_pair(predecessor, relevantState)));
                }
                for (auto successor : relevantSuccessors) {
                    expression = expression || variableInformation.statePairVariables.at(
                                                   variableInformation.statePairToIndexMap.at(std::make_pair(relevantState, successor)));
                }

                solver.add(expression);
            } else {
                // Assert the constraints (2).
                storm::storage::FlatSet<uint_fast64_t> relevantSuccessors;
                for (auto const& relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(relevantState)) {
                    for (auto const& successorEntry : transitionMatrix.getRow(relevantChoice)) {
                        if (relevantState != successorEntry.getColumn() &&
                            (relevancyInformation.relevantStates.get(successorEntry.getColumn()) || psiStates.get(successorEntry.getColumn()))) {
                            relevantSuccessors.insert(successorEntry.getColumn());
                        }
                    }
                }

                for (auto successor : relevantSuccessors) {
                    initialStateExpression =
                        initialStateExpression ||
                        variableInformation.statePairVariables.at(variableInformation.statePairToIndexMap.at(std::make_pair(relevantState, successor)));
                }
            }
        }
        solver.add(initialStateExpression);

        // Finally, add constraints that
        // (1) if a transition is selected, a valid labeling is selected as well.
        // (2) enforce that if a transition from s to s' is selected, the ordering variables become strictly larger.
        for (auto const& statePairIndexPair : variableInformation.statePairToIndexMap) {
            uint_fast64_t sourceState = statePairIndexPair.first.first;
            uint_fast64_t targetState = statePairIndexPair.first.second;

            // Assert constraint for (1).
            storm::storage::FlatSet<uint_fast64_t> choicesForStatePair;
            for (auto const& relevantChoice : relevancyInformation.relevantChoicesForRelevantStates.at(sourceState)) {
                for (auto const& successorEntry : transitionMatrix.getRow(relevantChoice)) {
                    if (successorEntry.getColumn() == targetState) {
                        choicesForStatePair.insert(relevantChoice);
                    }
                }
            }
            storm::expressions::Expression labelExpression = !variableInformation.statePairVariables.at(statePairIndexPair.second);
            for (auto choice : choicesForStatePair) {
                storm::expressions::Expression choiceExpression = variableInformation.manager->boolean(true);
                for (auto element : labelSets.at(choice)) {
                    if (relevancyInformation.knownLabels.find(element) == relevancyInformation.knownLabels.end()) {
                        choiceExpression = choiceExpression && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(element));
                    }
                }
                labelExpression = labelExpression || choiceExpression;
            }
            solver.add(labelExpression);

            // Assert constraint for (2).
            storm::expressions::Expression orderExpression =
                !variableInformation.statePairVariables.at(statePairIndexPair.second) ||
                variableInformation.stateOrderVariables.at(variableInformation.relevantStatesToOrderVariableIndexMap.at(sourceState)).getExpression() <
                    variableInformation.stateOrderVariables.at(variableInformation.relevantStatesToOrderVariableIndexMap.at(targetState)).getExpression();
            solver.add(orderExpression);
        }
    }

    /*!
     * Asserts that the disjunction of the given formulae holds. If the content of the disjunction is empty,
     * this corresponds to asserting false.
     *
     * @param solver The solver to use for the satisfiability evaluation.
     * @param formulaVector A vector of expressions that shall form the disjunction.
     * @param manager The expression manager to use.
     */
    static void assertDisjunction(storm::solver::SmtSolver& solver, std::vector<storm::expressions::Expression> const& formulaVector,
                                  storm::expressions::ExpressionManager const& manager) {
        storm::expressions::Expression disjunction = manager.boolean(false);
        for (auto expr : formulaVector) {
            disjunction = disjunction || expr;
        }
        solver.add(disjunction);
    }

    /*!
     * Creates a full-adder for the two inputs and returns the resulting bit as well as the carry bit.
     *
     * @param in1 The first input to the adder.
     * @param in2 The second input to the adder.
     * @param carryIn The carry bit input to the adder.
     * @return A pair whose first component represents the carry bit and whose second component represents the
     * result bit.
     */
    static std::pair<storm::expressions::Expression, storm::expressions::Expression> createFullAdder(storm::expressions::Expression in1,
                                                                                                     storm::expressions::Expression in2,
                                                                                                     storm::expressions::Expression carryIn) {
        storm::expressions::Expression resultBit;
        storm::expressions::Expression carryBit;

        if (carryIn.isFalse()) {
            resultBit = (in1 && !in2) || (!in1 && in2);
            carryBit = in1 && in2;
        } else {
            resultBit = (in1 && !in2 && !carryIn) || (!in1 && in2 && !carryIn) || (!in1 && !in2 && carryIn) || (in1 && in2 && carryIn);
            carryBit = in1 && in2 || in1 && carryIn || in2 && carryIn;
        }

        return std::make_pair(carryBit, resultBit);
    }

    /*!
     * Creates an adder for the two inputs of equal size. The resulting vector represents the different bits of
     * the sum (and is thus one bit longer than the two inputs).
     *
     * @param variableInformation The variable information structure.
     * @param in1 The first input to the adder.
     * @param in2 The second input to the adder.
     * @return A vector representing the bits of the sum of the two inputs.
     */
    static std::vector<storm::expressions::Expression> createAdder(VariableInformation const& variableInformation,
                                                                   std::vector<storm::expressions::Expression> const& in1,
                                                                   std::vector<storm::expressions::Expression> const& in2) {
        // Sanity check for sizes of input.
        if (in1.size() != in2.size() || in1.size() == 0) {
            STORM_LOG_ERROR("Illegal input to adder (" << in1.size() << ", " << in2.size() << ").");
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Illegal input to adder.");
        }

        // Prepare result.
        std::vector<storm::expressions::Expression> result;
        result.reserve(in1.size() + 1);

        // Add all bits individually and pass on carry bit appropriately.
        storm::expressions::Expression carryBit = variableInformation.manager->boolean(false);
        for (uint_fast64_t currentBit = 0; currentBit < in1.size(); ++currentBit) {
            std::pair<storm::expressions::Expression, storm::expressions::Expression> localResult = createFullAdder(in1[currentBit], in2[currentBit], carryBit);

            result.push_back(localResult.second);
            carryBit = localResult.first;
        }
        result.push_back(carryBit);

        return result;
    }

    /*!
     * Given a number of input numbers, creates a number of output numbers that corresponds to the sum of two
     * consecutive numbers of the input. If the number if input numbers is odd, the last number is simply added
     * to the output.
     *
     * @param variableInformation The variable information structure.
     * @param in A vector or binary encoded numbers.
     * @return A vector of numbers that each correspond to the sum of two consecutive elements of the input.
     */
    static std::vector<std::vector<storm::expressions::Expression>> createAdderPairs(VariableInformation const& variableInformation,
                                                                                     std::vector<std::vector<storm::expressions::Expression>> const& in) {
        std::vector<std::vector<storm::expressions::Expression>> result;

        uint64_t maxIndex = in.size() / 2;
        result.reserve(maxIndex + in.size() % 2);

        for (uint_fast64_t index = 0; index < maxIndex; ++index) {
            result.push_back(createAdder(variableInformation, in[2 * index], in[2 * index + 1]));
        }

        if (in.size() % 2 != 0) {
            result.push_back(in.back());
            result.back().push_back(variableInformation.manager->boolean(false));
        }

        return result;
    }

    /*!
     * Creates a counter circuit that returns the number of literals out of the given vector that are set to true.
     *
     * @param variableInformation The variable information structure.
     * @param literals The literals for which to create the adder circuit.
     * @return A bit vector representing the number of literals that are set to true.
     */
    static std::vector<storm::expressions::Expression> createCounterCircuit(VariableInformation const& variableInformation,
                                                                            std::vector<storm::expressions::Variable> const& literals) {
        if (literals.empty()) {
            return std::vector<storm::expressions::Expression>();
        }

        // Create the auxiliary vector.
        std::vector<std::vector<storm::expressions::Expression>> aux;
        for (uint_fast64_t index = 0; index < literals.size(); ++index) {
            aux.emplace_back();
            aux.back().push_back(literals[index]);
        }

        while (aux.size() > 1) {
            aux = createAdderPairs(variableInformation, aux);
        }

        STORM_LOG_DEBUG("Created counter circuit for " << literals.size() << " literals.");

        return aux.front();
    }

    /*!
     * Determines whether the bit at the given index is set in the given value.
     *
     * @param value The value to test.
     * @param index The index of the bit to test.
     * @return True iff the bit at the given index is set in the given value.
     */
    static bool bitIsSet(uint64_t value, uint64_t index) {
        uint64_t mask = 1 << (index & 63);
        return (value & mask) != 0;
    }

    /*!
     * Asserts a constraint in the given solver that expresses that the value encoded by the given input variables
     * may at most represent the number k. The constraint is associated with a relaxation variable, that is
     * returned by this function and may be used to deactivate the constraint.
     *
     * @param solver The solver to use for the satisfiability evaluation.
     * @param variableInformation The struct that holds the variable information.
     * @param k The bound for the binary-encoded value.
     * @return The relaxation variable associated with the constraint.
     */
    static storm::expressions::Variable assertLessOrEqualKRelaxed(storm::solver::SmtSolver& solver, VariableInformation const& variableInformation,
                                                                  uint64_t k) {
        STORM_LOG_DEBUG("Asserting solution has size less or equal " << k << ".");

        std::vector<storm::expressions::Variable> const& input = variableInformation.adderVariables;

        // If there are no input variables, the value is always 0 <= k, so there is nothing to assert.
        if (input.empty()) {
            std::stringstream variableName;
            variableName << "relaxed" << k;
            return variableInformation.manager->declareBooleanVariable(variableName.str());
        }

        storm::expressions::Expression result;
        if (bitIsSet(k, 0)) {
            result = variableInformation.manager->boolean(true);
        } else {
            result = !input.at(0);
        }
        for (uint_fast64_t index = 1; index < input.size(); ++index) {
            storm::expressions::Expression i1;
            storm::expressions::Expression i2;

            if (bitIsSet(k, index)) {
                i1 = !input.at(index);
                i2 = result;
            } else {
                i1 = variableInformation.manager->boolean(false);
                i2 = variableInformation.manager->boolean(false);
            }
            result = i1 || i2 || (!input.at(index) && result);
        }

        std::stringstream variableName;
        variableName << "relaxed" << k;
        storm::expressions::Variable relaxingVariable = variableInformation.manager->declareBooleanVariable(variableName.str());
        result = result || relaxingVariable;

        solver.add(result);

        return relaxingVariable;
    }

    /*!
     * Determines the set of labels that was chosen by the given model.
     *
     * @param model The model from which to extract the information.
     * @param variableInformation A structure with information about the variables of the solver.
     */
    static storm::storage::FlatSet<uint_fast64_t> getUsedLabelSet(storm::solver::SmtSolver::ModelReference const& model,
                                                                  VariableInformation const& variableInformation) {
        storm::storage::FlatSet<uint_fast64_t> result;
        for (auto const& labelIndexPair : variableInformation.labelToIndexMap) {
            bool commandIncluded = model.getBooleanValue(variableInformation.labelVariables.at(labelIndexPair.second));

            if (commandIncluded) {
                result.insert(labelIndexPair.first);
            }
        }
        return result;
    }

    /*!
     * Asserts an adder structure in the given solver that counts the number of variables that are set to true
     * out of the given variables.
     *
     * @param solver The solver for which to add the adder.
     * @param variableInformation A structure with information about the variables of the solver.
     */
    static std::vector<storm::expressions::Variable> assertAdder(storm::solver::SmtSolver& solver, VariableInformation const& variableInformation) {
        auto start = std::chrono::high_resolution_clock::now();
        std::stringstream variableName;
        std::vector<storm::expressions::Variable> result;

        std::vector<storm::expressions::Expression> adderVariables = createCounterCircuit(variableInformation, variableInformation.minimalityLabelVariables);
        for (uint_fast64_t i = 0; i < adderVariables.size(); ++i) {
            variableName.str("");
            variableName.clear();
            variableName << "adder" << i;
            result.push_back(variableInformation.manager->declareBooleanVariable(variableName.str()));
            solver.add(storm::expressions::implies(adderVariables[i], result.back()));
            STORM_LOG_TRACE("Added bit " << i << " of adder.");
        }

        auto end = std::chrono::high_resolution_clock::now();
        uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        STORM_LOG_DEBUG("Asserted adder in " << duration << "ms.");

        return result;
    }

    /*!
     * Finds the smallest set of labels such that the constraint system of the solver is still satisfiable.
     *
     * @param solver The solver to use for the satisfiability evaluation.
     * @param variableInformation A structure with information about the variables of the solver.
     * @param currentBound The currently known lower bound for the number of labels that need to be enabled
     * in order to satisfy the constraint system.
     * @return The smallest set of labels such that the constraint system of the solver is satisfiable.
     */
    static boost::optional<storm::storage::FlatSet<uint_fast64_t>> findSmallestCommandSet(storm::solver::SmtSolver& solver,
                                                                                          VariableInformation& variableInformation,
                                                                                          uint_fast64_t& currentBound) {
        // Check if we can find a solution with the current bound.
        storm::expressions::Expression assumption = !variableInformation.auxiliaryVariables.back();

        // As long as the constraints are unsatisfiable, we need to relax the last at-most-k constraint and
        // try with an increased bound.
        while (solver.checkWithAssumptions({assumption}) == storm::solver::SmtSolver::CheckResult::Unsat) {
            STORM_LOG_DEBUG("Constraint system is unsatisfiable with at most " << currentBound << " taken commands; increasing bound.");
            solver.add(variableInformation.auxiliaryVariables.back());
            variableInformation.auxiliaryVariables.push_back(assertLessOrEqualKRelaxed(solver, variableInformation, ++currentBound));
            assumption = !variableInformation.auxiliaryVariables.back();
            if (currentBound > variableInformation.minimalityLabelVariables.size()) {
                STORM_LOG_DEBUG("Constraint system fully explored: Bound exceeds maximum of " << variableInformation.minimalityLabelVariables.size());
                return boost::none;
            }
        }

        // At this point we know that the constraint system was satisfiable, so compute the induced label
        // set and return it.
        return getUsedLabelSet(*solver.getModel(), variableInformation);
    }

    static void ruleOutSingleSolution(storm::solver::SmtSolver& solver, storm::storage::FlatSet<uint_fast64_t> const& labelSet,
                                      VariableInformation& variableInformation, RelevancyInformation const& relevancyInformation) {
        std::vector<storm::expressions::Expression> formulae;

        storm::storage::FlatSet<uint_fast64_t> unknownLabels;
        std::set_intersection(labelSet.begin(), labelSet.end(), relevancyInformation.minimalityLabels.begin(), relevancyInformation.minimalityLabels.end(),
                              std::inserter(unknownLabels, unknownLabels.end()));

        // std::set_difference(labelSet.begin(), labelSet.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
        // std::inserter(unknownLabels, unknownLabels.end()));
        for (auto const& label : unknownLabels) {
            formulae.emplace_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
        }

        storm::storage::FlatSet<uint_fast64_t> remainingLabels;
        // std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(), labelSet.begin(), labelSet.end(),
        // std::inserter(remainingLabels, remainingLabels.end()));
        std::set_difference(relevancyInformation.minimalityLabels.begin(), relevancyInformation.minimalityLabels.end(), labelSet.begin(), labelSet.end(),
                            std::inserter(remainingLabels, remainingLabels.end()));

        for (auto const& label : remainingLabels) {
            formulae.emplace_back(variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
        }

        STORM_LOG_DEBUG("Ruling out single solution.");
        assertDisjunction(solver, formulae, *variableInformation.manager);
    }

    static void ruleOutBiggerSolutions(storm::solver::SmtSolver& solver, storm::storage::FlatSet<uint_fast64_t> const& labelSet,
                                       VariableInformation& variableInformation, RelevancyInformation const& relevancyInformation) {
        std::vector<storm::expressions::Expression> formulae;

        storm::storage::FlatSet<uint_fast64_t> unknownLabels;
        std::set_intersection(labelSet.begin(), labelSet.end(), relevancyInformation.minimalityLabels.begin(), relevancyInformation.minimalityLabels.end(),
                              std::inserter(unknownLabels, unknownLabels.end()));

        // std::set_difference(labelSet.begin(), labelSet.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
        // std::inserter(unknownLabels, unknownLabels.end()));
        for (auto const& label : unknownLabels) {
            formulae.emplace_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
        }

        STORM_LOG_DEBUG("Ruling out set of solutions.");
        assertDisjunction(solver, formulae, *variableInformation.manager);
    }

    /*!
     * Analyzes the given sub-model that has a maximal reachability of zero (i.e. no psi states are reachable) and tries to construct assertions that aim to
     * make at least one psi state reachable.
     *
     * @param solver The solver to use for the satisfiability evaluation.
     * @param subModel The sub-model resulting from restricting the original model to the given command set.
     * @param originalModel The original model.
     * @param phiStates A bit vector characterizing all phi states in the model.
     * @param psiState A bit vector characterizing all psi states in the model.
     * @param commandSet The currently chosen set of commands.
     * @param variableInformation A structure with information about the variables of the solver.
     */
    static void analyzeZeroProbabilitySolution(storm::solver::SmtSolver& solver, storm::models::sparse::Model<T> const& subModel,
                                               std::vector<storm::storage::FlatSet<uint_fast64_t>> const& subLabelSets,
                                               storm::models::sparse::Model<T> const& originalModel,
                                               std::vector<storm::storage::FlatSet<uint_fast64_t>> const& originalLabelSets,
                                               storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                               storm::storage::FlatSet<uint_fast64_t> const& commandSet, VariableInformation& variableInformation,
                                               RelevancyInformation const& relevancyInformation) {
        storm::storage::BitVector reachableStates(subModel.getNumberOfStates());

        STORM_LOG_DEBUG("Analyzing solution with zero probability.");

        // Initialize the stack for the DFS.
        bool targetStateIsReachable = false;
        std::vector<uint_fast64_t> stack;
        stack.reserve(subModel.getNumberOfStates());
        for (auto initialState : subModel.getInitialStates()) {
            stack.push_back(initialState);
            reachableStates.set(initialState, true);
        }

        storm::storage::SparseMatrix<T> const& transitionMatrix = subModel.getTransitionMatrix();
        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

        // Now determine which states and labels are actually reachable.
        storm::storage::FlatSet<uint_fast64_t> reachableLabels;
        while (!stack.empty()) {
            uint_fast64_t currentState = stack.back();
            stack.pop_back();

            for (uint_fast64_t currentChoice = nondeterministicChoiceIndices[currentState]; currentChoice < nondeterministicChoiceIndices[currentState + 1];
                 ++currentChoice) {
                bool choiceTargetsRelevantState = false;

                for (auto const& successorEntry : transitionMatrix.getRow(currentChoice)) {
                    if (relevancyInformation.relevantStates.get(successorEntry.getColumn()) && currentState != successorEntry.getColumn()) {
                        choiceTargetsRelevantState = true;
                        if (!reachableStates.get(successorEntry.getColumn())) {
                            reachableStates.set(successorEntry.getColumn());
                            stack.push_back(successorEntry.getColumn());
                        }
                    } else if (psiStates.get(successorEntry.getColumn())) {
                        targetStateIsReachable = true;
                    }
                }

                if (choiceTargetsRelevantState) {
                    for (auto label : subLabelSets[currentChoice]) {
                        reachableLabels.insert(label);
                    }
                }
            }
        }

        STORM_LOG_DEBUG("Successfully performed reachability analysis.");

        STORM_LOG_THROW(!targetStateIsReachable, storm::exceptions::InvalidStateException, "Target must be unreachable for this analysis.");

        storm::storage::BitVector unreachableRelevantStates = ~reachableStates & relevancyInformation.relevantStates;
        storm::storage::BitVector statesThatCanReachTargetStates =
            storm::utility::graph::performProbGreater0E(subModel.getBackwardTransitions(), phiStates, psiStates);

        storm::storage::FlatSet<uint_fast64_t> locallyRelevantLabels;
        std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(), commandSet.begin(), commandSet.end(),
                            std::inserter(locallyRelevantLabels, locallyRelevantLabels.begin()));

        std::vector<storm::storage::FlatSet<uint_fast64_t>> guaranteedLabelSets =
            storm::counterexamples::getGuaranteedLabelSets(originalModel, originalLabelSets, statesThatCanReachTargetStates, locallyRelevantLabels);
        STORM_LOG_DEBUG("Found " << reachableLabels.size() << " reachable labels and " << reachableStates.getNumberOfSetBits() << " reachable states.");

        // Search for states on the border of the reachable state space, i.e. states that are still reachable
        // and possess a (disabled) option to leave the reachable part of the state space.
        std::set<storm::storage::FlatSet<uint_fast64_t>> cutLabels;
        for (auto state : reachableStates) {
            for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(state)) {
                if (!std::includes(commandSet.begin(), commandSet.end(), originalLabelSets[currentChoice].begin(), originalLabelSets[currentChoice].end())) {
                    bool isBorderChoice = false;

                    // Determine whether the state has the option to leave the reachable state space and go to the unreachable relevant states.
                    for (auto const& successorEntry : originalModel.getTransitionMatrix().getRow(currentChoice)) {
                        if (unreachableRelevantStates.get(successorEntry.getColumn())) {
                            isBorderChoice = true;
                        }
                    }

                    if (isBorderChoice) {
                        storm::storage::FlatSet<uint_fast64_t> currentLabelSet;
                        std::set_difference(originalLabelSets[currentChoice].begin(), originalLabelSets[currentChoice].end(), commandSet.begin(),
                                            commandSet.end(), std::inserter(currentLabelSet, currentLabelSet.begin()));
                        std::set_difference(guaranteedLabelSets[state].begin(), guaranteedLabelSets[state].end(), commandSet.begin(), commandSet.end(),
                                            std::inserter(currentLabelSet, currentLabelSet.end()));

                        cutLabels.insert(currentLabelSet);
                    }
                }
            }
        }

        // Given the results of the previous analysis, we construct the implications.
        std::vector<storm::expressions::Expression> formulae;
        storm::storage::FlatSet<uint_fast64_t> unknownReachableLabels;
        std::set_difference(reachableLabels.begin(), reachableLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                            std::inserter(unknownReachableLabels, unknownReachableLabels.end()));
        storm::storage::FlatSet<uint64_t> unknownReachableMinimalityLabels;
        std::set_intersection(unknownReachableLabels.begin(), unknownReachableLabels.end(), relevancyInformation.minimalityLabels.begin(),
                              relevancyInformation.minimalityLabels.end(),
                              std::inserter(unknownReachableMinimalityLabels, unknownReachableMinimalityLabels.end()));

        for (auto label : unknownReachableMinimalityLabels) {
            formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
        }
        for (auto const& cutLabelSet : cutLabels) {
            storm::expressions::Expression cube = variableInformation.manager->boolean(true);
            for (auto cutLabel : cutLabelSet) {
                cube = cube && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(cutLabel));
            }

            formulae.push_back(cube);
        }

        STORM_LOG_DEBUG("Asserting reachability implications.");
        assertDisjunction(solver, formulae, *variableInformation.manager);
    }

    /*!
     * Analyzes the given sub-model that has a non-zero maximal reachability and tries to construct assertions that aim to guide the solver to solutions
     * with an improved probability value.
     *
     * @param solver The solver to use for the satisfiability evaluation.
     * @param subModel The sub-model resulting from restricting the original model to the given command set.
     * @param originalModel The original model.
     * @param phiStates A bit vector characterizing all phi states in the model.
     * @param psiState A bit vector characterizing all psi states in the model.
     * @param commandSet The currently chosen set of commands.
     * @param variableInformation A structure with information about the variables of the solver.
     */
    static void analyzeInsufficientProbabilitySolution(storm::solver::SmtSolver& solver, storm::models::sparse::Model<T> const& subModel,
                                                       std::vector<storm::storage::FlatSet<uint_fast64_t>> const& subLabelSets,
                                                       storm::models::sparse::Model<T> const& originalModel,
                                                       std::vector<storm::storage::FlatSet<uint_fast64_t>> const& originalLabelSets,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       storm::storage::FlatSet<uint_fast64_t> const& commandSet, VariableInformation& variableInformation,
                                                       RelevancyInformation const& relevancyInformation) {
        STORM_LOG_DEBUG("Analyzing solution with insufficient probability.");

        storm::storage::BitVector reachableStates(subModel.getNumberOfStates());

        // Initialize the stack for the DFS.
        std::vector<uint_fast64_t> stack;
        stack.reserve(subModel.getNumberOfStates());
        for (auto initialState : subModel.getInitialStates()) {
            stack.push_back(initialState);
            reachableStates.set(initialState, true);
        }

        storm::storage::SparseMatrix<T> const& transitionMatrix = subModel.getTransitionMatrix();
        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

        // Now determine which states and labels are actually reachable.
        storm::storage::FlatSet<uint_fast64_t> reachableLabels;
        while (!stack.empty()) {
            uint_fast64_t currentState = stack.back();
            stack.pop_back();

            for (uint_fast64_t currentChoice = nondeterministicChoiceIndices[currentState]; currentChoice < nondeterministicChoiceIndices[currentState + 1];
                 ++currentChoice) {
                bool choiceTargetsRelevantState = false;

                for (auto const& successorEntry : transitionMatrix.getRow(currentChoice)) {
                    if (relevancyInformation.relevantStates.get(successorEntry.getColumn()) && currentState != successorEntry.getColumn()) {
                        choiceTargetsRelevantState = true;
                        if (!reachableStates.get(successorEntry.getColumn())) {
                            reachableStates.set(successorEntry.getColumn(), true);
                            stack.push_back(successorEntry.getColumn());
                        }
                    }  // else if (psiStates.get(successorEntry.getColumn())) {
                       // targetStateIsReachable = true;
                    //}
                }

                if (choiceTargetsRelevantState) {
                    for (auto label : subLabelSets[currentChoice]) {
                        reachableLabels.insert(label);
                    }
                }
            }
        }
        STORM_LOG_DEBUG("Successfully determined reachable state space.");

        storm::storage::BitVector unreachableRelevantStates = ~reachableStates & relevancyInformation.relevantStates;
        storm::storage::BitVector statesThatCanReachTargetStates =
            storm::utility::graph::performProbGreater0E(subModel.getBackwardTransitions(), phiStates, psiStates);

        storm::storage::FlatSet<uint_fast64_t> locallyRelevantLabels;
        std::set_difference(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end(), commandSet.begin(), commandSet.end(),
                            std::inserter(locallyRelevantLabels, locallyRelevantLabels.begin()));

        std::vector<storm::storage::FlatSet<uint_fast64_t>> guaranteedLabelSets =
            storm::counterexamples::getGuaranteedLabelSets(originalModel, originalLabelSets, statesThatCanReachTargetStates, locallyRelevantLabels);

        // Search for states for which we could enable another option and possibly improve the reachability probability.
        std::set<storm::storage::FlatSet<uint_fast64_t>> cutLabels;
        for (auto state : reachableStates) {
            for (auto currentChoice : relevancyInformation.relevantChoicesForRelevantStates.at(state)) {
                if (!std::includes(commandSet.begin(), commandSet.end(), originalLabelSets[currentChoice].begin(), originalLabelSets[currentChoice].end())) {
                    storm::storage::FlatSet<uint_fast64_t> currentLabelSet;
                    std::set_difference(originalLabelSets[currentChoice].begin(), originalLabelSets[currentChoice].end(), commandSet.begin(), commandSet.end(),
                                        std::inserter(currentLabelSet, currentLabelSet.begin()));
                    std::set_difference(guaranteedLabelSets[state].begin(), guaranteedLabelSets[state].end(), commandSet.begin(), commandSet.end(),
                                        std::inserter(currentLabelSet, currentLabelSet.end()));

                    cutLabels.insert(currentLabelSet);
                }
            }
        }

        // Given the results of the previous analysis, we construct the implications
        std::vector<storm::expressions::Expression> formulae;
        storm::storage::FlatSet<uint_fast64_t> unknownReachableLabels;
        std::set_difference(reachableLabels.begin(), reachableLabels.end(), relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end(),
                            std::inserter(unknownReachableLabels, unknownReachableLabels.end()));
        storm::storage::FlatSet<uint64_t> unknownReachableMinimalityLabels;
        std::set_intersection(unknownReachableLabels.begin(), unknownReachableLabels.end(), relevancyInformation.minimalityLabels.begin(),
                              relevancyInformation.minimalityLabels.end(),
                              std::inserter(unknownReachableMinimalityLabels, unknownReachableMinimalityLabels.end()));

        for (auto label : unknownReachableMinimalityLabels) {
            formulae.push_back(!variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(label)));
        }
        for (auto const& cutLabelSet : cutLabels) {
            storm::expressions::Expression cube = variableInformation.manager->boolean(true);
            for (auto cutLabel : cutLabelSet) {
                cube = cube && variableInformation.labelVariables.at(variableInformation.labelToIndexMap.at(cutLabel));
            }

            formulae.push_back(cube);
        }

        STORM_LOG_DEBUG("Asserting reachability implications.");
        assertDisjunction(solver, formulae, *variableInformation.manager);
    }
#endif

    /*!
     * Returns the sub-model obtained from removing all choices that do not originate from the specified filterLabelSet.
     * Also returns the Labelsets of the sub-model.
     */
    static std::pair<std::shared_ptr<storm::models::sparse::Model<T>>, std::vector<storm::storage::FlatSet<uint_fast64_t>>> restrictModelToLabelSet(
        storm::models::sparse::Model<T> const& model, storm::storage::FlatSet<uint_fast64_t> const& filterLabelSet,
        boost::optional<uint64_t> absorbState = boost::none) {
        bool customRowGrouping = model.isOfType(storm::models::ModelType::Mdp);
        STORM_LOG_TRACE("Restrict model to label set " << storm::storage::toString(filterLabelSet));
        STORM_LOG_TRACE("Absorb state = " << (absorbState == boost::none ? "none" : std::to_string(absorbState.get())));
        std::vector<storm::storage::FlatSet<uint_fast64_t>> resultLabelSet;
        storm::storage::SparseMatrixBuilder<T> transitionMatrixBuilder(0, model.getTransitionMatrix().getColumnCount(), 0, true, customRowGrouping,
                                                                       model.getTransitionMatrix().getRowGroupCount());

        // Check for each choice of each state, whether the choice commands are fully contained in the given command set.
        uint_fast64_t currentRow = 0;
        for (uint_fast64_t state = 0; state < model.getNumberOfStates(); ++state) {
            bool stateHasValidChoice = false;
            for (uint_fast64_t choice = model.getTransitionMatrix().getRowGroupIndices()[state];
                 choice < model.getTransitionMatrix().getRowGroupIndices()[state + 1]; ++choice) {
                auto const& choiceLabelSet = model.getChoiceOrigins()->isPrismChoiceOrigins()
                                                 ? model.getChoiceOrigins()->asPrismChoiceOrigins().getCommandSet(choice)
                                                 : model.getChoiceOrigins()->asJaniChoiceOrigins().getEdgeIndexSet(choice);
                bool choiceValid = std::includes(filterLabelSet.begin(), filterLabelSet.end(), choiceLabelSet.begin(), choiceLabelSet.end());

                // If the choice is valid, copy over all its elements.
                if (choiceValid) {
                    STORM_LOG_TRACE("Choice " << choice << " has a valid label set " << storm::storage::toString(choiceLabelSet));

                    if (!stateHasValidChoice && customRowGrouping) {
                        transitionMatrixBuilder.newRowGroup(currentRow);
                    }
                    stateHasValidChoice = true;
                    for (auto const& entry : model.getTransitionMatrix().getRow(choice)) {
                        transitionMatrixBuilder.addNextValue(currentRow, entry.getColumn(), entry.getValue());
                    }
                    resultLabelSet.push_back(choiceLabelSet);
                    ++currentRow;
                }
            }

            // If no choice of the current state may be taken, we insert a self-loop to the state instead.
            if (!stateHasValidChoice) {
                if (customRowGrouping) {
                    transitionMatrixBuilder.newRowGroup(currentRow);
                }
                uint64_t targetState = (absorbState == boost::none ? state : absorbState.get());
                transitionMatrixBuilder.addNextValue(currentRow, targetState, storm::utility::one<T>());
                // Insert an empty label set for this choice
                resultLabelSet.emplace_back();
                ++currentRow;
            }
        }

        std::shared_ptr<storm::models::sparse::Model<T>> resultModel;
        if (model.isOfType(storm::models::ModelType::Dtmc)) {
            resultModel = std::make_shared<storm::models::sparse::Dtmc<T>>(
                transitionMatrixBuilder.build(), storm::models::sparse::StateLabeling(model.getStateLabeling()), model.getRewardModels());
        } else {
            resultModel = std::make_shared<storm::models::sparse::Mdp<T>>(
                transitionMatrixBuilder.build(), storm::models::sparse::StateLabeling(model.getStateLabeling()), model.getRewardModels());
        }

        return std::make_pair(resultModel, std::move(resultLabelSet));
    }

    static std::vector<T> computeMaximalReachabilityProbability(Environment const& env, storm::models::sparse::Model<T> const& model,
                                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                                boost::optional<std::vector<std::string>> const& rewardName) {
        std::vector<T> results;

        std::vector<T> allStatesResult;

        STORM_LOG_DEBUG("Invoking model checker on model with " << model.getNumberOfStates() << " states and " << model.getNumberOfTransitions()
                                                                << " transitions.");
        if (model.isOfType(storm::models::ModelType::Dtmc)) {
            if (rewardName == boost::none) {
                results.push_back(storm::utility::zero<T>());
                allStatesResult = storm::modelchecker::helper::SparseDtmcPrctlHelper<T>::computeUntilProbabilities(
                    env, false, model.getTransitionMatrix(), model.getBackwardTransitions(), phiStates, psiStates, false);
                for (auto state : model.getInitialStates()) {
                    STORM_LOG_TRACE("Found probability " << allStatesResult[state]);
                    results.back() = std::max(results.back(), allStatesResult[state]);
                }
                STORM_LOG_TRACE("Final probability " << results.back());
            } else {
                for (auto const& rewName : rewardName.get()) {
                    results.push_back(storm::utility::zero<T>());
                    allStatesResult = storm::modelchecker::helper::SparseDtmcPrctlHelper<T>::computeReachabilityRewards(
                        env, false, model.getTransitionMatrix(), model.getBackwardTransitions(), model.getRewardModel(rewName), psiStates, false);
                    for (auto state : model.getInitialStates()) {
                        results.back() = std::max(results.back(), allStatesResult[state]);
                    }
                }
            }
        } else {
            if (rewardName == boost::none) {
                results.push_back(storm::utility::zero<T>());
                storm::modelchecker::helper::SparseMdpPrctlHelper<T> modelCheckerHelper;
                allStatesResult = std::move(
                    modelCheckerHelper
                        .computeUntilProbabilities(env, false, model.getTransitionMatrix(), model.getBackwardTransitions(), phiStates, psiStates, false, false)
                        .values);
                for (auto state : model.getInitialStates()) {
                    results.back() = std::max(results.back(), allStatesResult[state]);
                }
            } else {
                STORM_LOG_THROW(rewardName != boost::none, storm::exceptions::NotSupportedException,
                                "Reward property counterexample generation is currently only supported for DTMCs.");
            }
        }

        return results;
    }

   public:
    struct Options {
        Options(bool checkThresholdFeasible = false) : checkThresholdFeasible(checkThresholdFeasible) {
            auto const& settings = storm::settings::getModule<storm::settings::modules::CounterexampleGeneratorSettings>();

            encodeReachability = settings.isEncodeReachabilitySet();
            useDynamicConstraints = settings.isUseDynamicConstraintsSet();
        }

        bool checkThresholdFeasible;
        bool encodeReachability;
        bool useDynamicConstraints;
        bool silent = false;
        bool addBackwardImplicationCuts = true;
        uint64_t continueAfterFirstCounterexampleUntil = 0;
        uint64_t maximumCounterexamples = 1;
        uint64_t multipleCounterexampleSizeCap = 100000000;
        uint64_t maximumExtraIterations = 100000000;
    };

    struct GeneratorStats {
        std::chrono::milliseconds setupTime;
        std::chrono::milliseconds solverTime;
        std::chrono::milliseconds modelCheckingTime;
        std::chrono::milliseconds analysisTime;
        std::chrono::milliseconds cutTime;
        uint64_t iterations;
    };

    /*!
     * Computes the minimal command set that is needed in the given model to exceed the given probability threshold for satisfying phi until psi.
     *
     * @param symbolicModel The symbolic model description that was used to build the model.
     * @param model The sparse model in which to find the minimal command set.
     * @param phiStates A bit vector characterizing all phi states in the model.
     * @param psiStates A bit vector characterizing all psi states in the model.
     * @param propertyThreshold The threshold that is to be achieved or exceeded.
     * @param rewardName The name of the reward structure to use, or boost::none if probabilities are considerd.
     * @param strictBound Indicates whether the threshold needs to be achieved (true) or exceeded (false).
     * @param options A set of options for customization.
     */
    static std::vector<storm::storage::FlatSet<uint_fast64_t>> getMinimalLabelSet(
        Environment const& env, GeneratorStats& stats, storm::storage::SymbolicModelDescription const& symbolicModel,
        storm::models::sparse::Model<T> const& model, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
        std::vector<double> propertyThreshold, boost::optional<std::vector<std::string>> const& rewardName, bool strictBound,
        storm::storage::FlatSet<uint_fast64_t> const& dontCareLabels = storm::storage::FlatSet<uint_fast64_t>(), Options const& options = Options()) {
#ifdef STORM_HAVE_Z3
        STORM_LOG_THROW(propertyThreshold.size() > 0, storm::exceptions::InvalidArgumentException, "At least one threshold has to be specified.");
        STORM_LOG_THROW(propertyThreshold.size() == 1 || (rewardName && rewardName.get().size() == propertyThreshold.size()),
                        storm::exceptions::InvalidArgumentException, "Multiple thresholds is only supported for multiple reward structures.");
        std::vector<storm::storage::FlatSet<uint_fast64_t>> result;
        // Set up all clocks used for time measurement.
        auto totalClock = std::chrono::high_resolution_clock::now();
        auto timeOfLastMessage = std::chrono::high_resolution_clock::now();
        decltype(std::chrono::high_resolution_clock::now() - totalClock) totalTime(0);

        auto setupTimeClock = std::chrono::high_resolution_clock::now();
        decltype(std::chrono::high_resolution_clock::now() - setupTimeClock) totalSetupTime(0);

        auto solverClock = std::chrono::high_resolution_clock::now();
        decltype(std::chrono::high_resolution_clock::now() - solverClock) totalSolverTime(0);

        auto modelCheckingClock = std::chrono::high_resolution_clock::now();
        decltype(std::chrono::high_resolution_clock::now() - modelCheckingClock) totalModelCheckingTime(0);

        auto analysisClock = std::chrono::high_resolution_clock::now();
        decltype(std::chrono::high_resolution_clock::now() - analysisClock) totalAnalysisTime(0);

        // (0) Obtain the label sets for each choice.
        // The label set of a choice corresponds to the set of prism commands that induce the choice.
        STORM_LOG_THROW(model.hasChoiceOrigins(), storm::exceptions::InvalidArgumentException,
                        "Restriction to minimal command set is impossible for model without choice origins.");
        STORM_LOG_THROW(model.getChoiceOrigins()->isPrismChoiceOrigins() || model.getChoiceOrigins()->isJaniChoiceOrigins(),
                        storm::exceptions::InvalidArgumentException, "Restriction to label set is impossible for model without PRISM or JANI choice origins.");

        std::vector<storm::storage::FlatSet<uint_fast64_t>> labelSets(model.getNumberOfChoices());
        if (model.getChoiceOrigins()->isPrismChoiceOrigins()) {
            storm::storage::sparse::PrismChoiceOrigins const& choiceOrigins = model.getChoiceOrigins()->asPrismChoiceOrigins();
            for (uint_fast64_t choice = 0; choice < model.getNumberOfChoices(); ++choice) {
                labelSets[choice] = choiceOrigins.getCommandSet(choice);
            }
        } else {
            storm::storage::sparse::JaniChoiceOrigins const& choiceOrigins = model.getChoiceOrigins()->asJaniChoiceOrigins();
            for (uint_fast64_t choice = 0; choice < model.getNumberOfChoices(); ++choice) {
                labelSets[choice] = choiceOrigins.getEdgeIndexSet(choice);
            }
        }
        STORM_LOG_ASSERT(labelSets.size() == model.getNumberOfChoices(), "Label set size mismatch.");

        // (1) Check whether its possible to exceed the threshold if checkThresholdFeasible is set.
        std::vector<double> maximalReachabilityProbability;
        if (options.checkThresholdFeasible) {
            maximalReachabilityProbability = computeMaximalReachabilityProbability(env, model, phiStates, psiStates, rewardName);

            for (uint64_t i = 0; i < maximalReachabilityProbability.size(); ++i) {
                STORM_LOG_THROW((strictBound && maximalReachabilityProbability[i] >= propertyThreshold[i]) ||
                                    (!strictBound && maximalReachabilityProbability[i] > propertyThreshold[i]),
                                storm::exceptions::InvalidArgumentException,
                                "Given probability threshold " << propertyThreshold[i] << " can not be " << (strictBound ? "achieved" : "exceeded")
                                                               << " in model with maximal reachability probability of " << maximalReachabilityProbability[i]
                                                               << ".");
                std::cout << "\nMaximal property value in model is " << maximalReachabilityProbability[i] << ".\n\n";
            }
        }

        // (2) Identify all states and commands that are relevant, because only these need to be considered later.
        RelevancyInformation relevancyInformation = determineRelevantStatesAndLabels(model, labelSets, phiStates, psiStates, dontCareLabels);

        // (3) Create a solver.
        std::shared_ptr<storm::expressions::ExpressionManager> manager = std::make_shared<storm::expressions::ExpressionManager>();
        std::unique_ptr<storm::solver::SmtSolver> solver = std::make_unique<storm::solver::Z3SmtSolver>(*manager);

        // (4) Create the variables for the relevant commands.
        VariableInformation variableInformation = createVariables(manager, model, psiStates, relevancyInformation, options.encodeReachability);
        STORM_LOG_DEBUG("Created variables.");

        // (5) Now assert an adder whose result variables can later be used to constrain the nummber of label
        // variables that were set to true. Initially, we are looking for a solution that has no label enabled
        // and subsequently relax that.
        variableInformation.adderVariables = assertAdder(*solver, variableInformation);
        variableInformation.auxiliaryVariables.push_back(assertLessOrEqualKRelaxed(*solver, variableInformation, 0));

        // As we are done with the setup at this point, stop the clock for the setup time.
        totalSetupTime = std::chrono::high_resolution_clock::now() - setupTimeClock;

        // (6) Add constraints that cut off a lot of suboptimal solutions.
        STORM_LOG_DEBUG("Asserting cuts.");
        stats.cutTime =
            assertCuts(symbolicModel, model, labelSets, psiStates, variableInformation, relevancyInformation, *solver, options.addBackwardImplicationCuts);
        STORM_LOG_DEBUG("Asserted cuts.");
        if (options.encodeReachability) {
            assertReachabilityCuts(model, labelSets, psiStates, variableInformation, relevancyInformation, *solver);
            STORM_LOG_DEBUG("Asserted reachability cuts.");
        }

        // (7) Find the smallest set of commands that satisfies all constraints. If the probability of
        // satisfying phi until psi exceeds the given threshold, the set of labels is minimal and can be returned.
        // Otherwise, the current solution has to be ruled out and the next smallest solution is retrieved from
        // the solver.

        storm::storage::FlatSet<uint_fast64_t> commandSet(relevancyInformation.knownLabels);

        // If there are no relevant labels, return directly.
        if (relevancyInformation.relevantLabels.empty()) {
            return {commandSet};
        } else if (relevancyInformation.minimalityLabels.empty()) {
            commandSet.insert(relevancyInformation.relevantLabels.begin(), relevancyInformation.relevantLabels.end());
            return {commandSet};
        }

        // Set up some variables for the iterations.
        bool done = false;
        uint_fast64_t lastSize = 0;
        uint_fast64_t iterations = 0;
        uint_fast64_t currentBound = 0;
        uint64_t firstCounterexampleFound = 0;  // The value is not queried before being set.
        std::vector<double> maximalPropertyValue;
        uint_fast64_t zeroProbabilityCount = 0;
        size_t smallestCounterexampleSize = model.getNumberOfChoices();  // Definitive upper bound
        uint64_t progressDelay = storm::settings::getModule<storm::settings::modules::GeneralSettings>().getShowProgressDelay();
        do {
            ++iterations;

            if (result.size() > 0 && iterations > firstCounterexampleFound + options.maximumExtraIterations) {
                break;
            }
            if (result.size() == 0) {
                STORM_LOG_DEBUG("Sanity check to see whether constraint system is still satisfiable.");
                STORM_LOG_ASSERT(solver->check() == storm::solver::SmtSolver::CheckResult::Sat, "Constraint system is not satisfiable anymore.");
            }
            STORM_LOG_DEBUG("Computing minimal command set.");
            solverClock = std::chrono::high_resolution_clock::now();
            boost::optional<storm::storage::FlatSet<uint_fast64_t>> smallest = findSmallestCommandSet(*solver, variableInformation, currentBound);
            totalSolverTime += std::chrono::high_resolution_clock::now() - solverClock;
            if (smallest == boost::none) {
                STORM_LOG_DEBUG("No further counterexamples.");
                break;
            } else {
                commandSet = smallest.get();
            }
            STORM_LOG_DEBUG("Computed minimal command with bound " << currentBound << " and set of size "
                                                                   << commandSet.size() + relevancyInformation.knownLabels.size() << " (" << commandSet.size()
                                                                   << " + " << relevancyInformation.knownLabels.size() << ") ");

            // Restrict the given model to the current set of labels and compute the reachability probability.
            modelCheckingClock = std::chrono::high_resolution_clock::now();
            commandSet.insert(relevancyInformation.knownLabels.begin(), relevancyInformation.knownLabels.end());
            commandSet.insert(relevancyInformation.dontCareLabels.begin(), relevancyInformation.dontCareLabels.end());
            if (commandSet.size() > smallestCounterexampleSize + options.continueAfterFirstCounterexampleUntil ||
                (result.size() > 1 && commandSet.size() > options.multipleCounterexampleSizeCap)) {
                STORM_LOG_DEBUG("No further counterexamples of similar size.");
                break;
            }

            if (commandSet.size() == nrCommands(symbolicModel)) {
                result.push_back(commandSet);
                break;
            }

            auto subChoiceOrigins = restrictModelToLabelSet(model, commandSet, rewardName ? boost::make_optional(psiStates.getNextSetIndex(0)) : boost::none);
            std::shared_ptr<storm::models::sparse::Model<T>> const& subModel = subChoiceOrigins.first;
            std::vector<storm::storage::FlatSet<uint_fast64_t>> const& subLabelSets = subChoiceOrigins.second;

            // Now determine the maximal reachability probability in the sub-model.
            maximalPropertyValue = computeMaximalReachabilityProbability(env, *subModel, phiStates, psiStates, rewardName);
            totalModelCheckingTime += std::chrono::high_resolution_clock::now() - modelCheckingClock;

            // Depending on whether the threshold was successfully achieved or not, we proceed by either analyzing the bad solution or stopping the iteration
            // process.
            analysisClock = std::chrono::high_resolution_clock::now();
            bool violation = false;
            for (uint64_t i = 0; i < maximalPropertyValue.size(); i++) {
                violation |=
                    (strictBound && maximalPropertyValue[i] < propertyThreshold[i]) || (!strictBound && maximalPropertyValue[i] <= propertyThreshold[i]);
            }

            if (violation) {
                if (!rewardName && maximalPropertyValue.front() == storm::utility::zero<T>()) {
                    ++zeroProbabilityCount;
                }

                if (options.useDynamicConstraints) {
                    // Determine which of the two analysis techniques to call by performing a reachability analysis.
                    storm::storage::BitVector reachableStates =
                        storm::utility::graph::getReachableStates(subModel->getTransitionMatrix(), subModel->getInitialStates(), phiStates, psiStates);

                    if (reachableStates.isDisjointFrom(psiStates)) {
                        // If there was no target state reachable, analyze the solution and guide the solver into the right direction.
                        analyzeZeroProbabilitySolution(*solver, *subModel, subLabelSets, model, labelSets, phiStates, psiStates, commandSet,
                                                       variableInformation, relevancyInformation);
                    } else {
                        // If the reachability probability was greater than zero (i.e. there is a reachable target state), but the probability was insufficient
                        // to exceed the given threshold, we analyze the solution and try to guide the solver into the right direction.
                        analyzeInsufficientProbabilitySolution(*solver, *subModel, subLabelSets, model, labelSets, phiStates, psiStates, commandSet,
                                                               variableInformation, relevancyInformation);
                    }

                    if (relevancyInformation.dontCareLabels.size() > 0) {
                        ruleOutSingleSolution(*solver, commandSet, variableInformation, relevancyInformation);
                    }
                } else {
                    // Do not guide solver, just rule out current solution.
                    ruleOutSingleSolution(*solver, commandSet, variableInformation, relevancyInformation);
                }
            } else {
                STORM_LOG_DEBUG("Found a counterexample.");
                if (result.empty()) {
                    // If this is the first counterexample we find, we store when we found it.
                    firstCounterexampleFound = iterations;
                }
                result.push_back(commandSet);
                if (options.maximumCounterexamples > result.size()) {
                    STORM_LOG_DEBUG("Exclude counterexample for future.");
                    ruleOutBiggerSolutions(*solver, commandSet, variableInformation, relevancyInformation);
                } else {
                    STORM_LOG_DEBUG("Stop searching for further counterexamples.");
                    done = true;
                }
                smallestCounterexampleSize = std::min(smallestCounterexampleSize, commandSet.size());
            }
            totalAnalysisTime += (std::chrono::high_resolution_clock::now() - analysisClock);

            auto now = std::chrono::high_resolution_clock::now();
            auto durationSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(now - timeOfLastMessage).count();
            if (static_cast<uint64_t>(durationSinceLastMessage) >= progressDelay || lastSize < commandSet.size()) {
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - totalClock).count();
                if (lastSize < commandSet.size()) {
                    STORM_LOG_DEBUG("Improved lower bound to " << currentBound << " after " << milliseconds << "ms.");
                    lastSize = commandSet.size();
                } else {
                    STORM_LOG_DEBUG("Lower bound on label set size is " << currentBound << " after " << milliseconds << "ms (checked " << iterations
                                                                        << " models, " << zeroProbabilityCount << " could not reach the target set).");
                    timeOfLastMessage = std::chrono::high_resolution_clock::now();
                }
            }
        } while (!done);

        // Compute and emit the time measurements if the corresponding flag was set.
        totalTime = std::chrono::high_resolution_clock::now() - totalClock;

        stats.analysisTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalAnalysisTime);
        stats.setupTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalSetupTime);
        stats.modelCheckingTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalModelCheckingTime);
        stats.solverTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalSolverTime);
        stats.iterations = iterations;

        {
            storm::storage::FlatSet<uint64_t> allLabels;
            for (auto const& e : labelSets) {
                allLabels.insert(e.begin(), e.end());
            }

            STORM_LOG_STATISTICS("Metrics:\n");
            STORM_LOG_STATISTICS("    * all labels: " << allLabels.size() << '\n');
            STORM_LOG_STATISTICS("    * known labels: " << relevancyInformation.knownLabels.size() << '\n');
            STORM_LOG_STATISTICS("    * relevant labels: " << (relevancyInformation.knownLabels.size() + relevancyInformation.relevantLabels.size()) << "\n\n");
            STORM_LOG_STATISTICS("Time breakdown:\n");
            STORM_LOG_STATISTICS("    * time for setup: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalSetupTime).count() << "ms\n");
            STORM_LOG_STATISTICS("    * time for solving: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalSolverTime).count() << "ms\n");
            STORM_LOG_STATISTICS("    * time for checking: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalModelCheckingTime).count()
                                                             << "ms\n");
            STORM_LOG_STATISTICS("    * time for analysis: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalAnalysisTime).count() << "ms\n");
            STORM_LOG_STATISTICS("------------------------------------------\n");
            STORM_LOG_STATISTICS("    * total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalTime).count() << "ms\n\n");
            STORM_LOG_STATISTICS("Other:\n");
            STORM_LOG_STATISTICS("    * number of models checked: " << iterations << '\n');
            STORM_LOG_STATISTICS("    * number of models that could not reach a target state: "
                                 << zeroProbabilityCount << " (" << 100 * static_cast<double>(zeroProbabilityCount) / iterations << "%)\n\n");
        }

        return result;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This functionality is unavailable since storm has been compiled without support for Z3.");
#endif
    }

    static void extendLabelSetLowerBound(storm::models::sparse::Model<T> const& model, storm::storage::FlatSet<uint_fast64_t>& commandSet,
                                         storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, bool silent = false) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // Create sub-model that only contains the choices allowed by the given command set.
        std::shared_ptr<storm::models::sparse::Model<T>> subModel = restrictModelToLabelSet(model, commandSet).first;

        // Then determine all prob0E(psi) states that are reachable in the sub-model.
        storm::storage::BitVector reachableProb0EStates =
            storm::utility::graph::getReachableStates(subModel->getTransitionMatrix(), subModel->getInitialStates(), phiStates, psiStates);

        // Create a queue of reachable prob0E(psi) states so we can check which commands need to be added
        // to give them a strategy that avoids psi states.
        std::queue<uint_fast64_t> prob0EWorklist;
        for (auto e : reachableProb0EStates) {
            prob0EWorklist.push(e);
        }

        // As long as there are reachable prob0E(psi) states, we add commands so they can stay within
        // prob0E(states).
        while (!prob0EWorklist.empty()) {
            uint_fast64_t state = prob0EWorklist.front();
            prob0EWorklist.pop();

            // Now iterate over the original choices of the prob0E(psi) state and add at least one.
            bool hasLabeledChoice = false;
            uint64_t smallestCommandSetSize = 0;
            uint64_t smallestCommandChoice = model.getTransitionMatrix().getRowGroupIndices()[state];

            // Determine the choice with the least amount of commands (bad heuristic).
            for (uint64_t choice = smallestCommandChoice; choice < model.getTransitionMatrix().getRowGroupIndices()[state + 1]; ++choice) {
                bool onlyProb0ESuccessors = true;
                for (auto const& successorEntry : model.getTransitionMatrix().getRow(choice)) {
                    if (!psiStates.get(successorEntry.getColumn())) {
                        onlyProb0ESuccessors = false;
                        break;
                    }
                }

                if (onlyProb0ESuccessors) {
                    uint64_t labelSetSize = 0;
                    if (model.getChoiceOrigins()->isPrismChoiceOrigins()) {
                        labelSetSize = model.getChoiceOrigins()->asPrismChoiceOrigins().getCommandSet(choice).size();
                    } else {
                        STORM_LOG_ASSERT(model.getChoiceOrigins()->isJaniChoiceOrigins(), "Expected JANI choice origins.");
                        labelSetSize = model.getChoiceOrigins()->asJaniChoiceOrigins().getEdgeIndexSet(choice).size();
                    }
                    hasLabeledChoice |= (labelSetSize != 0);

                    if (smallestCommandChoice == 0 || labelSetSize < smallestCommandSetSize) {
                        smallestCommandSetSize = labelSetSize;
                        smallestCommandChoice = choice;
                    }
                }
            }

            if (hasLabeledChoice) {
                // Take all labels of the selected choice.
                if (model.getChoiceOrigins()->isPrismChoiceOrigins()) {
                    auto const& labelSet = model.getChoiceOrigins()->asPrismChoiceOrigins().getCommandSet(smallestCommandChoice);
                    commandSet.insert(labelSet.begin(), labelSet.end());
                } else {
                    STORM_LOG_ASSERT(model.getChoiceOrigins()->isJaniChoiceOrigins(), "Expected JANI choice origins.");
                    auto const& labelSet = model.getChoiceOrigins()->asJaniChoiceOrigins().getEdgeIndexSet(smallestCommandChoice);
                    commandSet.insert(labelSet.begin(), labelSet.end());
                }

                // Check for which successor states choices need to be added
                for (auto const& successorEntry : model.getTransitionMatrix().getRow(smallestCommandChoice)) {
                    if (!storm::utility::isZero(successorEntry.getValue())) {
                        if (!reachableProb0EStates.get(successorEntry.getColumn())) {
                            reachableProb0EStates.set(successorEntry.getColumn());
                            prob0EWorklist.push(successorEntry.getColumn());
                        }
                    }
                }
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        if (!silent) {
            std::cout << "\nExtended command for lower bounded property to size " << commandSet.size() << " in "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms.\n";
        }
    }

    struct CexInput {
        storm::logic::ComparisonType comparisonType;
        std::vector<double> threshold;
        boost::optional<std::vector<std::string>> rewardName = boost::none;
        bool lowerBoundedFormula = false;
        bool strictBound;
        storm::storage::BitVector phiStates;
        storm::storage::BitVector psiStates;

        void addRewardThresholdCombination(std::string reward, double thresh) {
            STORM_LOG_THROW(rewardName, storm::exceptions::InvalidOperationException, "Can only add more reward names if a reward name is already set.");
            rewardName.get().push_back(reward);
            threshold.push_back(thresh);
        }
    };

    static CexInput precompute(Environment const& env, storm::storage::SymbolicModelDescription const& symbolicModel,
                               storm::models::sparse::Model<T> const& model, std::shared_ptr<storm::logic::Formula const> const& formula) {
        CexInput result;
        STORM_LOG_THROW(formula->isProbabilityOperatorFormula() || formula->isRewardOperatorFormula(), storm::exceptions::InvalidPropertyException,
                        "Counterexample generation does not support this kind of formula. Expecting a probability operator as the outermost formula element.");
        if (formula->isProbabilityOperatorFormula()) {
            storm::logic::ProbabilityOperatorFormula const& probabilityOperator = formula->asProbabilityOperatorFormula();
            STORM_LOG_THROW(probabilityOperator.hasBound(), storm::exceptions::InvalidPropertyException,
                            "Counterexample generation only supports bounded formulas.");
            STORM_LOG_THROW(probabilityOperator.getSubformula().isUntilFormula() || probabilityOperator.getSubformula().isEventuallyFormula(),
                            storm::exceptions::InvalidPropertyException,
                            "Path formula is required to be of the form 'phi U psi' for counterexample generation.");

            result.comparisonType = probabilityOperator.getComparisonType();
            result.threshold.push_back(probabilityOperator.getThresholdAs<T>());
        } else {
            STORM_LOG_ASSERT(formula->isRewardOperatorFormula(), "Expected reward operator formula.");
            storm::logic::RewardOperatorFormula const& rewardOperator = formula->asRewardOperatorFormula();
            STORM_LOG_THROW(rewardOperator.hasBound(), storm::exceptions::InvalidPropertyException,
                            "Counterexample generation only supports bounded formulas.");
            STORM_LOG_THROW(rewardOperator.getSubformula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                            "Path formula is required to be of the form 'F phi' for counterexample generation.");

            result.comparisonType = rewardOperator.getComparisonType();
            result.threshold.push_back(rewardOperator.getThresholdAs<T>());
            result.rewardName = std::vector<std::string>();
            result.rewardName.get().push_back(rewardOperator.getRewardModelName());

            STORM_LOG_THROW(!storm::logic::isLowerBound(result.comparisonType), storm::exceptions::NotSupportedException,
                            "Lower bounds in counterexamples are only supported for probability formulas.");
            STORM_LOG_THROW(model.hasRewardModel(result.rewardName.get().front()), storm::exceptions::InvalidPropertyException,
                            "Property refers to reward " << result.rewardName.get().front() << " but model does not contain such a reward model.");
            STORM_LOG_THROW(model.getRewardModel(result.rewardName.get().front()).hasOnlyStateRewards(), storm::exceptions::NotSupportedException,
                            "We only support state-based rewards at the moment.");
        }
        result.strictBound = result.comparisonType == storm::logic::ComparisonType::Less;
        storm::logic::Formula const& subformula = formula->asOperatorFormula().getSubformula();

        storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<T>> modelchecker(model);

        if (subformula.isUntilFormula()) {
            STORM_LOG_THROW(!storm::logic::isLowerBound(result.comparisonType), storm::exceptions::NotSupportedException,
                            "Lower bounds in counterexamples are only supported for eventually formulas.");
            storm::logic::UntilFormula const& untilFormula = subformula.asUntilFormula();

            std::unique_ptr<storm::modelchecker::CheckResult> leftResult = modelchecker.check(env, untilFormula.getLeftSubformula());
            std::unique_ptr<storm::modelchecker::CheckResult> rightResult = modelchecker.check(env, untilFormula.getRightSubformula());

            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& leftQualitativeResult = leftResult->template asExplicitQualitativeCheckResult<T>();
            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& rightQualitativeResult = rightResult->template asExplicitQualitativeCheckResult<T>();

            result.phiStates = leftQualitativeResult.getTruthValuesVector();
            result.psiStates = rightQualitativeResult.getTruthValuesVector();
        } else if (subformula.isEventuallyFormula()) {
            storm::logic::EventuallyFormula const& eventuallyFormula = subformula.asEventuallyFormula();

            std::unique_ptr<storm::modelchecker::CheckResult> subResult = modelchecker.check(env, eventuallyFormula.getSubformula());

            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& subQualitativeResult = subResult->template asExplicitQualitativeCheckResult<T>();

            result.phiStates = storm::storage::BitVector(model.getNumberOfStates(), true);
            result.psiStates = subQualitativeResult.getTruthValuesVector();
        }

        if (storm::logic::isLowerBound(result.comparisonType)) {
            STORM_LOG_DEBUG("Computing counterexample for a lowerbound.");
            // If the formula specifies a lower bound, we need to modify the phi and psi states.
            // More concretely, we convert P(min)>lambda(F psi) to P(max)<(1-lambda)(G !psi) = P(max)<(1-lambda)(!psi U prob0E(psi))
            // where prob0E(psi) is the set of states for which there exists a strategy \sigma_0 that avoids
            // reaching psi states completely.

            // This means that from all states in prob0E(psi) we need to include labels such that \sigma_0
            // is actually included in the resulting model. This prevents us from guaranteeing the minimality of
            // the returned counterexample, so we warn about that.

            // Modify bound appropriately.
            result.comparisonType = storm::logic::invertPreserveStrictness(result.comparisonType);
            result.threshold.back() = storm::utility::one<T>() - result.threshold.back();

            // Modify the phi and psi states appropriately.
            storm::storage::BitVector statesWithProbability0E =
                storm::utility::graph::performProb0E(model.getTransitionMatrix(), model.getTransitionMatrix().getRowGroupIndices(),
                                                     model.getBackwardTransitions(), result.phiStates, result.psiStates);
            result.phiStates = ~result.psiStates;
            result.psiStates = std::move(statesWithProbability0E);

            // Remember our transformation so we can add commands to guarantee that the prob0E(a) states actually
            // have a strategy that voids a states.
            result.lowerBoundedFormula = true;
        }
        return result;
    }

    static std::vector<storm::storage::FlatSet<uint_fast64_t>> computeCounterexampleLabelSet(
        Environment const& env, GeneratorStats& stats, storm::storage::SymbolicModelDescription const& symbolicModel,
        storm::models::sparse::Model<T> const& model, CexInput const& counterexInput,
        storm::storage::FlatSet<uint_fast64_t> const& dontCareLabels = storm::storage::FlatSet<uint_fast64_t>(), Options const& options = Options(true)) {
        STORM_LOG_THROW(model.isOfType(storm::models::ModelType::Dtmc) || model.isOfType(storm::models::ModelType::Mdp),
                        storm::exceptions::NotSupportedException, "MaxSAT-based counterexample generation is supported only for discrete-time models.");

        // Delegate the actual computation work to the function of equal name.
        auto startTime = std::chrono::high_resolution_clock::now();
        auto labelSets = getMinimalLabelSet(env, stats, symbolicModel, model, counterexInput.phiStates, counterexInput.psiStates, counterexInput.threshold,
                                            counterexInput.rewardName, counterexInput.strictBound, dontCareLabels, options);
        auto endTime = std::chrono::high_resolution_clock::now();
        if (!options.silent) {
            for (auto const& labelSet : labelSets) {
                std::cout << "\nComputed minimal label set of size " << labelSet.size();
            }
            std::cout << "\n in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms.\n";
        }

        // Extend the command set properly.
        for (auto& labelSet : labelSets) {
            if (counterexInput.lowerBoundedFormula) {
                STORM_LOG_DEBUG("Extending the counterexample due to lower bound computation.");
                extendLabelSetLowerBound(model, labelSet, counterexInput.phiStates, counterexInput.psiStates, options.silent);
            }
        }
        return labelSets;
    }

    static std::shared_ptr<HighLevelCounterexample> computeCounterexample(Environment const& env, storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                          storm::models::sparse::Model<T> const& model,
                                                                          std::shared_ptr<storm::logic::Formula const> const& formula) {
#ifdef STORM_HAVE_Z3
        std::cout << "\nGenerating minimal label counterexample for formula " << *formula << '\n';
        GeneratorStats stats;
        CexInput prec = precompute(env, symbolicModel, model, formula);
        if (prec.lowerBoundedFormula) {
            STORM_LOG_WARN("Generating counterexample for lower-bounded property. The resulting command set need not be minimal.");
        }
        auto labelSets = computeCounterexampleLabelSet(env, stats, symbolicModel, model, prec);

        if (symbolicModel.isPrismProgram()) {
            storm::prism::Program program = symbolicModel.asPrismProgram().restrictCommands(labelSets[0]);
            if (formula->isProbabilityOperatorFormula()) {
                program.removeRewardModels();
            }
            return std::make_shared<HighLevelCounterexample>(program);
        } else {
            STORM_LOG_ASSERT(symbolicModel.isJaniModel(), "Unknown symbolic model description type.");
            return std::make_shared<HighLevelCounterexample>(symbolicModel.asJaniModel().restrictEdges(labelSets[0]));
        }
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This functionality is unavailable since storm has been compiled without support for Z3.");
        return nullptr;
#endif
    }
};

}  // namespace counterexamples
}  // namespace storm
